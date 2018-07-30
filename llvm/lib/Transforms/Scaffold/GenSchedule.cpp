//===----------------- GenSched.cpp ----------------------===//
// This file implements the Scaffold Pass of counting the number 
//  of critical timesteps and gate parallelism in program
//  in callgraph post-order.
//
//        This file was created by Scaffold Compiler Working Group
// Fine-grained list scheduling for leaf modules
// Coarse-grained scheduling for non-leaf modules
// Get T gate proportion within schedule length
// Cleaned up the code
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "GenSched"
#include <vector>
#include <deque>
#include <iostream> 
#include <limits>
#include <map>
#include <string>
#include <sstream>
#include <utility>
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Constants.h"
#include "llvm/Module.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/LLVMContext.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Support/CFG.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Argument.h"
#include "llvm/ADT/ilist.h"
#include "llvm/Constants.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
//#include "llvm/ScheduleDAG.h"


using namespace llvm;
using namespace std;

static cl::opt<unsigned>
LPFS_READ("sched-read", cl::init(0), cl::Hidden,
  cl::desc("sched schedule exists, read it in instead"));

static cl::opt<unsigned>
RES_CONSTRAINT("simd-kconstraint-sched", cl::init(8), cl::Hidden,
  cl::desc("k in SIMD-k Resource Constrained Scheduling"));

static cl::opt<unsigned>
DATA_CONSTRAINT("simd-dconstraint-sched", cl::init(1024), cl::Hidden,
  cl::desc("d in SIMD-d Resource Constrained Scheduling"));

static cl::opt<unsigned>
SIMD_L_SCHED("simd_l_sched", cl::init(1), cl::Hidden,
  cl::desc("l value for longest path first (sched)"));


#define MAX_RES_CONSTRAINT 2000 
#define SSCHED_THRESH 10000000

#define MAX_GATE_ARGS 100000
#define MAX_BT_COUNT 15 //max backtrace allowed - to avoid infinite recursive loops
#define NUM_QGATES 18
#define _CNOT 0
#define _H 1
#define _S 2
#define _T 3
#define _X 4
#define _Y 5
#define _Z 6
#define _MeasX 7
#define _MeasZ 8
#define _PrepX 9
#define _PrepZ 10
#define _Tdag 11
#define _Sdag 12
#define _Rz 13
#define _Toffoli 14
#define _Fredkin 15
#define _All 16
#define _CZ 17

bool debugGenSched = false; 

namespace {
	struct qArgInfo{
	  	string name;
	  	int index;
	  	int id;
	  	int simd;
	  	int loc;
	  	int nextTS;
	  	Instruction* last_inst;
	  	qArgInfo(): name("none"), index(-1), id(-1), simd(0), loc(0), nextTS(-1), last_inst(NULL) { }
	  	bool operator == (const qArgInfo& a) const{
	  		return (name == a.name && index == a.index);
	  	}
	};
	struct qGateArg{ //arguments to qgate calls
  		Value* argPtr;
  		int argNum;
  		bool isQbit;
  		bool isCbit;
		bool isAbit;
  		bool isUndef;
  		bool isPtr;
  		int valOrIndex; //Value if not Qbit, Index if Qbit & not a Ptr
  		double angle;
  		qGateArg(): argPtr(NULL), argNum(-1), isQbit(false), isCbit(false), isAbit(false), isUndef(false),
		  isPtr(false), valOrIndex(-1), angle(0.0){ }
  	};
	struct qGate{
  		Function* qFunc;
 		int numArgs;
 		vector<qArgInfo> args;
 		double angle;
 		qGate():qFunc(NULL), numArgs(0), angle(0.0) { }
	};
	struct op{
	  	string name;
		qGate gate;
	  	int id;
	  	int ts;
	  	int dist;
	  	bool followed;
		bool isIntrinsic;
	  	int simd;
		int length;
	  	Instruction* inst;
	  	vector<op*> in_edges;
	  	vector<op*> out_edges;
	  	op(): name(),gate(),id(-1),ts(-1),dist(1),followed(0),isIntrinsic(0),simd(-1),length(1),inst(NULL),in_edges(),out_edges(){}
	};
	struct qubit{
		qArgInfo val;
	  	op* last_op;
	  	qubit():val(),last_op() { }
	};
	struct move{
	  	int ts;
	  	int src;
	  	int dest;
	  	qArgInfo arg;
	  	move():ts(-1),src(-1),dest(-1),arg() { }
	};
  	struct GenSched : public ModulePass {
    	static char ID; // Pass identification

		unsigned long long parallelism; //Statistics tracking: num parallelized lines
		unsigned long long GlobalParallelism = 0; //Statistics tracking: num parallelized lines
    	int btCount; //backtrace count
		int op_cnt; //count of operations for each function
		int ts_cnt; //count of timesteps required for each function 
    	string gate_name[NUM_QGATES];
		vector<op*> programOps;
    	vector<op*> funcOps;  
    	vector<qGateArg*> tmpDepQbit;
		vector<Value*> vectQbits;
		map<Function*,int> opLengths;
    	map<string, map<int,uint64_t> > funcQbits; //qbits in current function
		vector<qubit*> qubits;
		deque<op*> longestPath;
		deque<op*> rdyQ;
		vector<deque<op*> > schedule;
    	map<string, int> gate_index;    

    	GenSched() : ModulePass(ID) {}

    	void init_gate_names(){
    	    gate_name[_CNOT] = "CNOT";
    	    gate_name[_H] = "H";
    	    gate_name[_S] = "S";
    	    gate_name[_T] = "T";
    	    gate_name[_Toffoli] = "Toffoli";
    	    gate_name[_X] = "X";
    	    gate_name[_Y] = "Y";
    	    gate_name[_Z] = "Z";
    	    gate_name[_MeasX] = "MeasX";
    	    gate_name[_MeasZ] = "MeasZ";
    	    gate_name[_PrepX] = "PrepX";
    	    gate_name[_PrepZ] = "PrepZ";
    	    gate_name[_Sdag] = "Sdag";
    	    gate_name[_Tdag] = "Tdag";
    	    gate_name[_Fredkin] = "Fredkin";
    	    gate_name[_Rz] = "Rz";
    	    gate_name[_All] = "All";                    
            gate_name[_CZ] = "CZ";
    	    
    	    gate_index["CNOT"] = _CNOT;        
    	    gate_index["H"] = _H;
    	    gate_index["S"] = _S;
    	    gate_index["T"] = _T;
    	    gate_index["Toffoli"] = _Toffoli;
    	    gate_index["X"] = _X;
    	    gate_index["Y"] = _Y;
    	    gate_index["Z"] = _Z;
    	    gate_index["Sdag"] = _Sdag;
    	    gate_index["Tdag"] = _Tdag;
    	    gate_index["MeasX"] = _MeasX;
    	    gate_index["MeasZ"] = _MeasZ;
    	    gate_index["PrepX"] = _PrepX;
    	    gate_index["PrepZ"] = _PrepZ;
    	    gate_index["Fredkin"] = _Fredkin;
    	    gate_index["Rz"] = _Rz;
    	    gate_index["All"] = _All;                    
            gate_index["CZ"] = _CZ;
    	    }

		string contractGateNames(string& name){
//			for(vector<op*>::iterator it = funcOps.begin(); it != funcOps.end(); ++it){
//				string name = (*it)->name;	
				bool intrinsic = false;
				if(name.find("llvm.CNOT") != string::npos){
					intrinsic = true; name = "CNOT";
				}
				else if(name.find("llvm.PrepZ") != string::npos){
				   	intrinsic = true; name = "PrepZ";
				}
				else if(name.find("llvm.PrepX") != string::npos){
				   	intrinsic = true; name = "PrepX";
				}
				else if(name.find("llvm.H") != string::npos){
				   	intrinsic = true; name = "H";
				}
				else if(name.find("llvm.Sdag") != string::npos){
				   	intrinsic = true; name = "Sdag";
				}
				else if(name.find("llvm.Tdag") != string::npos){
				   	intrinsic = true; name = "Tdag";
				}
				else if(name.find("llvm.Toffoli") != string::npos){
				   	intrinsic = true; name = "Toffoli";
				}
				else if(name.find("llvm.X") != string::npos){
				   	intrinsic = true; name = "X";
				}
				else if(name.find("llvm.Y") != string::npos){
				   	intrinsic = true; name = "Y";
				}
				else if(name.find("llvm.Z") != string::npos){
					intrinsic = true; name = "Z";
				}
				else if(name.find("llvm.S") != string::npos){
				   	intrinsic = true; name = "S";
				}
				else if(name.find("llvm.T") != string::npos){
				   	intrinsic = true; name = "T";
				}
				else if(name.find("llvm.MeasX") != string::npos){
				   	intrinsic = true; name = "MeasX";
				}
				else if(name.find("llvm.MeasZ") != string::npos){
				   	intrinsic = true; name = "MeasZ";
				}
				else if(name.find("llvm.Fredkin") != string::npos){
				   	intrinsic = true; name = "Fredkin";
				}
				else if(name.find("llvm.Rz") != string::npos){
					intrinsic = true; name = "Rz";
				}
				else if(name.find("llvm.") != string::npos){
					intrinsic = true; name = name.substr(5);
				}
				else if(name.find("CZ") != string::npos){
					intrinsic = true; name = "CZ";
				}

		//		(*it)->name = name;
		//		(*it)->isIntrinsic = intrinsic;
		//	}
			return name;
		}

		void contractAllGateNames(){
			for(vector<op*>::iterator it = funcOps.begin(); it != funcOps.end(); ++it){
				string name = (*it)->name;	
				bool intrinsic = false;
				if(name.find("llvm.CNOT") != string::npos){
					intrinsic = true; name = "CNOT";
				}
                else if(name.find("llvm.CZ") != string::npos || name.find("CZ") != string::npos){
                    intrinsic = true; name = "CZ";
                }
				else if(name.find("llvm.PrepZ") != string::npos){
				   	intrinsic = true; name = "PrepZ";
				}
				else if(name.find("llvm.PrepX") != string::npos){
				   	intrinsic = true; name = "PrepX";
				}
				else if(name.find("llvm.H") != string::npos){
				   	intrinsic = true; name = "H";
				}
				else if(name.find("llvm.Sdag") != string::npos){
				   	intrinsic = true; name = "Sdag";
				}
				else if(name.find("llvm.Tdag") != string::npos){
				   	intrinsic = true; name = "Tdag";
				}
				else if(name.find("llvm.Toffoli") != string::npos){
				   	intrinsic = true; name = "Toffoli";
				}
				else if(name.find("llvm.X") != string::npos){
				   	intrinsic = true; name = "X";
				}
				else if(name.find("llvm.Y") != string::npos){
				   	intrinsic = true; name = "Y";
				}
				else if(name.find("llvm.Z") != string::npos){
					intrinsic = true; name = "Z";
				}
				else if(name.find("llvm.S") != string::npos){
				   	intrinsic = true; name = "S";
				}
				else if(name.find("llvm.T") != string::npos){
				   	intrinsic = true; name = "T";
				}
				else if(name.find("llvm.MeasX") != string::npos){
				   	intrinsic = true; name = "MeasX";
				}
				else if(name.find("llvm.MeasZ") != string::npos){
				   	intrinsic = true; name = "MeasZ";
				}
				else if(name.find("llvm.Fredkin") != string::npos){
				   	intrinsic = true; name = "Fredkin";
				}
				else if(name.find("llvm.Rz") != string::npos){
					intrinsic = true; name = "Rz";
				}
				else if(name.find("llvm.") != string::npos){
					intrinsic = true; name = name.substr(5);
				}
				(*it)->name = name;
				(*it)->isIntrinsic = intrinsic;
			}
		}



	Instruction* findNextCallInst(BasicBlock *BB, BasicBlock::iterator ii);
	Instruction* buildModule(deque<op*>& timeStep, Module *M);
    void init_gates_as_functions();    
//	void getFunctionArguments(Function* F, vector<qGateArg*>& functionArguments);
	void getFunctionArguments(Function* F);
	void getFunctionType(op* newOp,int op_count);
    bool backtraceOperand(Value* opd, int opOrIndex);
	void printHeader(Function* F, int op_cnt, int ts_cnt);
	void printFuncOps();
	void print_qGate(qGate qg);
	void printDependencyGraph();
	void printLongestPath();
	int  printSchedule();
	void processSchedule(Module *M);
	void printRuntime(int runtime, string& funcName);
	void printScheduledGate(op* newOp);
	void printOpTrace(op* newOp, qubit* qbit);
	void printQubitTrace();
	int  processInstructions(Function* F);
	void checkForDependencies(op* newOp);
	void analyzeCallInst(CallInst* CI, op* newOp);
	void analyzeAllocInst(AllocaInst* AI);
	bool depsMet(op* newOp, int currentTS);
	void sched(Function* F);
    bool runOnModule (Module &M);    
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    	AU.setPreservesAll();  
      	AU.addRequired<CallGraph>();
//		AU.addRequired<MemoryDependenceAnalysis>();
    }


  }; // End of struct GenSched
} // End of anonymous namespace

// Identification of Pass for Integration 
char GenSched::ID = 0;
static RegisterPass<GenSched> X("GenSchedule", "Generate Parallel Schedule");

//GenSched Function Implementations
void GenSched::init_gates_as_functions(){
  	for(int  i =0; i< NUM_QGATES ; i++){
    	string gName = gate_name[i];
    	string fName = "llvm.";
    	fName.append(gName);
  	}
}
bool GenSched::backtraceOperand(Value* opd, int opOrIndex){
	if(opOrIndex == 0){//backtrace for operand
      	//search for opd in qbit/cbit vector
      	vector<Value*>::iterator vIter=find(vectQbits.begin(),vectQbits.end(),opd);
      	if(vIter != vectQbits.end()){
      	  	tmpDepQbit[0]->argPtr = opd;
      	  	return true;
      	}
		if(btCount>MAX_BT_COUNT)
     		return false;
      	if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(opd)){
          	if(GEPI->hasAllConstantIndices()){
            	Instruction* pInst = dyn_cast<Instruction>(opd);
            	unsigned numOps = pInst->getNumOperands();
            	backtraceOperand(pInst->getOperand(0),0);
            //NOTE: getelemptr instruction can have multiple indices. Currently considering last operand as desired index for qubit. Check this reasoning. 
            	if(ConstantInt *CI = dyn_cast<ConstantInt>(pInst->getOperand(numOps-1))){
              		if(tmpDepQbit.size()==1){
						//Accumulate indices for multi-references
                		tmpDepQbit[0]->valOrIndex += CI->getZExtValue();         
					}
            	}
          	}
          	else if(GEPI->hasIndices()){
            	Instruction* pInst = dyn_cast<Instruction>(opd);
            	unsigned numOps = pInst->getNumOperands();
            	backtraceOperand(pInst->getOperand(0),0);
            	if((tmpDepQbit[0]->isQbit || tmpDepQbit[0]->isAbit) && !(tmpDepQbit[0]->isPtr)){     
              		//NOTE: getelemptr instruction can have multiple indices. consider last operand as desired index for qubit. Check if this is true for all.
              		backtraceOperand(pInst->getOperand(numOps-1),1);
            	}
          	}
          	else{     
            	Instruction* pInst = dyn_cast<Instruction>(opd);
            	unsigned numOps = pInst->getNumOperands();
            	for(unsigned iop=0;iop<numOps;iop++){
            	  	backtraceOperand(pInst->getOperand(iop),0);
            	}
          	}
          	return true;
        }
      
      	if(Instruction* pInst = dyn_cast<Instruction>(opd)){
        	unsigned numOps = pInst->getNumOperands();
        	for(unsigned iop=0;iop<numOps;iop++){
        	  	btCount++;
        	  	backtraceOperand(pInst->getOperand(iop),0);
        	  	btCount--;
        	}
        	return true;
      	}
      	else{
        	return true;
      	}
	}
  	else if(opOrIndex == 0){ //opOrIndex == 1; i.e. Backtracing for Index    
    	if(btCount>MAX_BT_COUNT) //prevent infinite backtracing
      		return true;
    	if(ConstantInt *CI = dyn_cast<ConstantInt>(opd)){
      		tmpDepQbit[0]->valOrIndex = CI->getZExtValue();
      		return true;
    	}      
    	if(Instruction* pInst = dyn_cast<Instruction>(opd)){
      		unsigned numOps = pInst->getNumOperands();
      		for(unsigned iop=0;iop<numOps;iop++){
        		btCount++;
        		backtraceOperand(pInst->getOperand(iop),1);
        		btCount--;
      		}
    	}
  	}
  	else{ //opOrIndex == 2: backtracing to call inst MeasZ
    	if(CallInst *endCI = dyn_cast<CallInst>(opd)){
      		if(endCI->getCalledFunction()->getName().find("llvm.Meas") != string::npos){
        		tmpDepQbit[0]->argPtr = opd;
        		return true;
      		}
      		else{
        		if(Instruction* pInst = dyn_cast<Instruction>(opd)){
          			unsigned numOps = pInst->getNumOperands();
          			bool foundOne=false;
          			for(unsigned iop=0;(iop<numOps && !foundOne);iop++){
          			  	btCount++;
          			  	foundOne = foundOne || backtraceOperand(pInst->getOperand(iop),2);
          			  	btCount--;
          			}
          			return foundOne;
        		}
      		}
    	}
    	else{
      		if(Instruction* pInst = dyn_cast<Instruction>(opd)){
        		unsigned numOps = pInst->getNumOperands();
        		bool foundOne=false;
        		for(unsigned iop=0;(iop<numOps && !foundOne);iop++){
        		  	btCount++;
        		  	foundOne = foundOne || backtraceOperand(pInst->getOperand(iop),2);
        		  	btCount--;
        		}
        		return foundOne;
      		}
    	}
  	}
  	return false;
}
//void GenSched::getFunctionArguments(Function* F, vector<qGateArg*>& functionArguments){
void GenSched::getFunctionArguments(Function* F){
	for(Function::arg_iterator ait=F->arg_begin();ait!=F->arg_end();++ait){    
    	string argName = (ait->getName()).str();
      	Type* argType = ait->getType();
      	unsigned int argNum = ait->getArgNo();         
      	qGateArg tmpQArg;
      	tmpQArg.argPtr = ait;
      	tmpQArg.argNum = argNum;
      	if(argType->isPointerTy()){
      	  	tmpQArg.isPtr = true;
      	  	Type *elementType = argType->getPointerElementType();
      	  	if (elementType->isIntegerTy(16)){ //qbit*
      	  	  	tmpQArg.isQbit = true;
//      	  	  	functionArguments.push_back(&tmpQArg);
	      	  	vectQbits.push_back(ait);
//      	  	  	map<int,uint64_t> tmpMap;
//      	  	  	tmpMap[-1] = 0; //add entry for entire array
//      	  	  	tmpMap[-2] = 0; //add entry for max     
//      	  	  	funcQbits[argName]=tmpMap;      
//      	  	  	funcArgs[argName] = argNum;
      	  	}
			else if (elementType->isIntegerTy(8)){ //qbit*
      	  	  	tmpQArg.isAbit = true;
//	     	  	  	functionArguments.push_back(&tmpQArg);
	      	  	vectQbits.push_back(ait);
//      	  	  	map<int,uint64_t> tmpMap;
//      	  	  	tmpMap[-1] = 0; //add entry for entire array
//      	  	  	tmpMap[-2] = 0; //add entry for max     
//      	  	  	funcQbits[argName]=tmpMap;      
//      	  	  	funcArgs[argName] = argNum;
      	  	}
      	  	else if (elementType->isIntegerTy(1)){ //cbit*
      	  	  	tmpQArg.isCbit = true;
  //    	  	  	functionArguments.push_back(&tmpQArg);
      	  	  	vectQbits.push_back(ait);
//      	  	  	funcArgs[argName] = argNum;
      	  	}
      	}
      	else if (argType->isIntegerTy(16)){ //qbit
      	  	tmpQArg.isQbit = true;
//      	  	functionArguments.push_back(&tmpQArg);
      	  	vectQbits.push_back(ait);
      	    map<int,uint64_t> tmpMap;
      	    tmpMap[-1] = 0; //add entry for entire array
      	    tmpMap[-2] = 0; //add entry for max
      	}
		else if (argType->isIntegerTy(8)){ //cbit
      		tmpQArg.isAbit = true;
//      	  	functionArguments.push_back(&tmpQArg);
      	  	vectQbits.push_back(ait);
      	}
      	else if (argType->isIntegerTy(1)){ //cbit
      		tmpQArg.isCbit = true;
//      	  	functionArguments.push_back(&tmpQArg);
      	  	vectQbits.push_back(ait);
      	}
    }
}
void GenSched::getFunctionType(op* newOp, int op_count){
	if(newOp->name.find("llvm.") == string::npos){
		newOp->isIntrinsic = false;
		newOp->length = op_count;
	}
}
void GenSched::printHeader(Function* F, int op_cnt, int ts_cnt){
	errs() << "\nLPFS:\n";
	errs() << "Function: " << F->getName() << " (sched: sched, k: " 
	  << RES_CONSTRAINT << ", d: " << DATA_CONSTRAINT << " l: " << SIMD_L_SCHED 
	  << ", ops: " << op_cnt << ", ts: " << ts_cnt << ") \n"; 
	errs() << "==================================================================\n";
}
int GenSched::printSchedule(){
	ts_cnt = 0;
	for(unsigned long i = 0; i < schedule.size(); i++){
		map<Function*,int>::iterator it = opLengths.find(schedule[i].front()->gate.qFunc);
		if(it != opLengths.end()) ts_cnt += it->second;
		else ts_cnt += schedule[i].front()->length;
		int psm = schedule[i].size();
//        if(schedule[i].size() > 1) errs() << "pb();\n";
		for(deque<op*>::iterator it = schedule[i].begin(); it != schedule[i].end(); ++it){
			errs() <<i <<","<< psm << " " << (*it)->name << " ";
			for(int i = 0; i < (*it)->gate.numArgs; ++i){
				errs() << (*it)->gate.args[i].name << (*it)->gate.args[i].index;
				if((*it)->gate.numArgs > i + 1) errs() << " ";
			}
			errs() << "\n";
		}
//        if(schedule[i].size() > 1) errs() << "pe();\n";
	}
	errs() << "\n";
	return ts_cnt;
}
Instruction* GenSched::findNextCallInst(BasicBlock *BB, BasicBlock::iterator ii){
	for(; ii != BB->end(); ++ii){
		if (CallInst *CI = dyn_cast<CallInst>(ii)){
			return ii;
		}
	}	
	return NULL;
}
Instruction* GenSched::buildModule(deque<op*>& timeStep, Module *M){
	string FuncName = "llvmv_";
	vector<Type*> FunctionTypes;
	vector<Value*> FunctionArgs;
	for(deque<op*>::iterator it = timeStep.begin(); it != timeStep.end(); it++){
		CallInst *CI = dyn_cast<CallInst>((*it)->inst);
		string gateName = CI->getCalledFunction()->getName();
		FuncName = FuncName + contractGateNames(gateName) + "_";		
		for(int i = 0; i < CI->getNumArgOperands(); i++){
			FunctionArgs.push_back(CI->getArgOperand(i));
			FunctionTypes.push_back(CI->getArgOperand(i)->getType());
			if(CI->getArgOperand(i)->getType()->isIntegerTy(16)) FuncName = FuncName + "i16_";
			else if(CI->getArgOperand(i)->getType()->isIntegerTy(8)) FuncName = FuncName + "i8_";
		}
	}
	FunctionType *FuncType = FunctionType::get( Type::getVoidTy(getGlobalContext()),
		ArrayRef<Type*>(FunctionTypes), false);
	Function *DR = M->getFunction(FuncName);
	if (!DR) {
//		errs() << "Creating function: " << FuncName << " with " << FunctionArgs.size() << " args \n";
//		DR->setName(FuncName);
		DR = Function::Create(FuncType, GlobalVariable::ExternalLinkage,FuncName, M);
		BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "", DR, 0);
		ReturnInst::Create(getGlobalContext(), 0, BB);
	}
	Instruction* I = timeStep.front()->inst;
	Instruction *J = CallInst::Create(DR, ArrayRef<Value*>(FunctionArgs));
	BasicBlock::iterator ii(I);
//	ReplaceInstWithInst(I->getParent()->getInstList(), ii, J);	
	return J;
}
void GenSched::processSchedule(Module *M){
	BasicBlock *BB = schedule[0].front()->inst->getParent(); 
//	TerminatorInst *term = BB->getTerminator();
	Instruction *term = BB->getTerminator();
	for(unsigned long i = 0; i < schedule.size(); i++){
        for(unsigned long j = 0; j < schedule[i].size(); j++){
            Instruction *I = schedule[i][j]->inst;
            I->moveBefore(term);
        }
//		if(schedule[i].size() > 1){
//			Instruction *I = buildModule(schedule[i], M);
//			I->insertBefore(term);
//			for(deque<op*>::iterator it = schedule[i].begin(); it != schedule[i].end(); ++it){
//				(*it)->inst->eraseFromParent();
//			}
//		}
//		else{
//			schedule[i].front()->inst->moveBefore(term);
//		}
	}
}
void GenSched::printScheduledGate(op* newOp){
	errs() << newOp->ts << ",1 " << newOp->name << " ";
	for(int i = 0; i < newOp->gate.numArgs; ++i){
		errs() << newOp->gate.args[i].name << newOp->gate.args[i].index;
		if(newOp->gate.numArgs > i + 1) errs() << " ";
	}
	errs() << "\n";
}
void GenSched::printRuntime(int runtime, string& funcName){
	errs() << "#Num of SIMD time steps for function " << funcName << " : " << runtime << "\n";
}
int GenSched::processInstructions(Function* F){
	int op_count = 0;
	for (inst_iterator I = inst_begin(*F), E = inst_end(*F); I != E; ++I) {
	    Instruction *Inst = &*I;
	    if(CallInst *CI = dyn_cast<CallInst>(Inst)){
	        op* newOp = new op;
 	        string called_func_name = CI->getCalledFunction()->getName();
			newOp->name = called_func_name;
			newOp->inst = Inst;	
			analyzeCallInst(CI, newOp);
			checkForDependencies(newOp);
			newOp->id = funcOps.size();
        	funcOps.push_back(newOp);
		}
		else if (AllocaInst *AI = dyn_cast<AllocaInst>(Inst)) {
			analyzeAllocInst(AI);          
		}
	}
	return op_count;
}
void GenSched::checkForDependencies(op* newOp){
//	errs() << "in sequence dependency checking: "; print_qGate(newOp->gate);
	bool foundDep = false;
	for(int i = 0; i < newOp->gate.numArgs; i++){
		bool found = false;
		for(vector<qubit*>::iterator it = qubits.begin(); it != qubits.end(); it++){
			if((*it)->val == newOp->gate.args[i]){ //Found qubit in list
//				errs() << "\tgot a dependency with:";print_qGate((*it)->last_op->gate);
				newOp->in_edges.push_back((*it)->last_op);
				(*it)->last_op->out_edges.push_back(newOp);
				(*it)->last_op = newOp;
				found = true;
				foundDep = true;
				break;
			}
		}
		if(!found){
			qubit* newQbit = new qubit;
			newQbit->val = newOp->gate.args[i];
			newQbit->last_op = newOp;
			qubits.push_back(newQbit);
		}
	}
	if(!foundDep) rdyQ.push_front(newOp);
}
void GenSched::printDependencyGraph(){
	errs() << "IDs\n";
	for(vector<op*>::iterator it = funcOps.begin(); it != funcOps.end(); it++){
		errs() << (*it)->id << "\t" ; print_qGate((*it)->gate);
	}
	errs() << "Dependency Graph:\n";
	for(vector<op*>::iterator it = funcOps.begin(); it != funcOps.end(); it++){
		if((*it)->name == "store_cbit") continue;
		//errs() << (*it)->inst << "\t"; print_qGate((*it)->gate);
		//errs() << "ID: " << (*it)->id << "\n";
		//errs() << (*it)->id << " ";
		//errs() << "Number of dependencies:\n\tIn: " << (*it)->in_edges.size()<< "\n\tOut: "<<(*it)->out_edges.size()<<"\n";
		//errs() << "\tIn Edges: \n";
	   	//for(vector<op*>::iterator mit = (*it)->in_edges.begin(); mit != (*it)->in_edges.end(); mit++){
		//	errs() << "\t";
		//	errs() << (*mit)->inst << "\t"; print_qGate((*mit)->gate);
		//}
		//errs() << "\tOut Edges: \n";
		for(vector<op*>::iterator mit = (*it)->out_edges.begin(); mit != (*it)->out_edges.end(); mit++){
//			errs() << "\t";
//			errs() << (*mit)->inst << "\t"; print_qGate((*mit)->gate);
			errs() << (*it)->id << " " << (*mit)->id << "\n";
		}
	}
	errs() << "End of Dependency Graph\n";
}
void GenSched::printLongestPath(){
	errs() << "Longest Path (length:" << longestPath.size() << ")\n";
	for(deque<op*>::iterator it = longestPath.begin(); it != longestPath.end(); ++it){
		errs() << "\t";
		print_qGate((*it)->gate);
	}
}
void GenSched::printFuncOps(){
	errs() << "Printing function operations: total ops: " << funcOps.size() << "\n";
	for(vector<op*>::iterator it = funcOps.begin(); it != funcOps.end(); it++){
//		if((*it)->name == "store_cbit") continue;
		print_qGate((*it)->gate);
		errs() << "\tID: " << (*it)->id<<"\n";
		errs() << "\tDist: " << (*it)->dist<<"\n";
		errs() << "\tTS: " << (*it)->ts<<"\n";
		errs() << "\tFollowed? " << (*it)->followed<<"\n";
		errs() << "\tSIMD: " << (*it)->simd<<"\n";
		errs() << "\tIn Edges:\n";
		for(vector<op*>::iterator mit = (*it)->in_edges.begin(); mit != (*it)->in_edges.end(); ++mit){
			errs() << "\t\t"; errs() << (*mit)->id << ":";print_qGate((*mit)->gate);
		}
		errs() << "\tOut Edges:\n";
		for(vector<op*>::iterator mit = (*it)->out_edges.begin(); mit != (*it)->out_edges.end(); ++mit){
			errs() << "\t\t"; errs() << (*mit)->id << ":";print_qGate((*mit)->gate);
		}
	}
}
void GenSched::print_qGate(qGate qg){
	errs() << qg.qFunc->getName() << " : ";
	for(int i=0;i<qg.numArgs;i++){
	  	errs() << qg.args[i].name << qg.args[i].index << ", "  ;
	}
	errs() << "\n";
}
//TODO Remove these two traces from the final scheduler
void GenSched::printOpTrace(op* newOp, qubit* qbit){
	errs() << "\t" << newOp->id << ":";
	print_qGate(newOp->gate);
	for(vector<op*>::iterator mit = newOp->in_edges.begin(); mit != newOp->in_edges.end();++mit){
		for(int i = 0; i < (*mit)->gate.numArgs; i++){
			if((*mit)->gate.args[i] == (*qbit).val) printOpTrace((*mit),qbit);
		}
	}
}
void GenSched::printQubitTrace(){
	for(vector<qubit*>::iterator it = qubits.begin(); it != qubits.end(); ++it){
		errs() << "Qubit Trace: " << (*it)->val.name << (*it)->val.index << "\n\t" << (*it)->last_op->id << ":";
		print_qGate((*it)->last_op->gate);
		for(vector<op*>::iterator mit = (*it)->last_op->in_edges.begin(); mit != (*it)->last_op->in_edges.end();++mit){
			for(int i = 0; i < (*mit)->gate.numArgs; i++){
				if((*mit)->gate.args[i] == (*it)->val) printOpTrace((*mit),(*it));
			}
		}
	}
}
void GenSched::analyzeAllocInst(AllocaInst *AI){
	Type *allocatedType = AI->getAllocatedType();
	if(ArrayType *arrayType = dyn_cast<ArrayType>(allocatedType)) {      
//		qGateArg tmpQArg;
	  	Type *elementType = arrayType->getElementType();
	  	uint64_t arraySize = arrayType->getNumElements();
	  	if (elementType->isIntegerTy(16)){
	  	  	vectQbits.push_back(AI);
//	  	  	tmpQArg.isQbit = true;
//	  	  	tmpQArg.argPtr = AI;
//	  	  	tmpQArg.valOrIndex = arraySize;
//	  	  	map<int,uint64_t> tmpMap; //add qbit to funcQbits
//	  	  	tmpMap[-1] = 0; //entry for entire array ops
//	  	  	tmpMap[-2] = 0; //entry for max
//	  	  	funcQbits[AI->getName()]=tmpMap;
	  	}
		else if (elementType->isIntegerTy(8)){
			vectQbits.push_back(AI);
//			tmpQArg.isAbit = true;
//			tmpQArg.argPtr = AI;
//			tmpQArg.valOrIndex = arraySize;
//			map<int,uint64_t> tmpMap; //add qbit to funcQbits
//	    	tmpMap[-1] = 0; //entry for entire array ops
//	    	tmpMap[-2] = 0; //entry for max
//	    	funcQbits[AI->getName()]=tmpMap;
	  	}
	  	if (elementType->isIntegerTy(1)){
	    	vectQbits.push_back(AI); //Cbit added here
//	    	tmpQArg.isCbit = true;
//	    	tmpQArg.argPtr = AI;
//	    	tmpQArg.valOrIndex = arraySize;
	  	}
	}
}
void GenSched::analyzeCallInst(CallInst* CI, op* newOp){
	if(debugGenSched)
    	errs() << "Call inst: " << CI->getCalledFunction()->getName() << "\n";
//    if(CI->getCalledFunction()->getName() == "store_cbit"){   //trace return values
//    	return;
//    }      
	vector<qGateArg> allDepQbit;                                  
    bool tracked_all_operands = true;
    int myPrepState = -1;
    double myRotationAngle = 0.0;
    for(unsigned iop=0;iop<CI->getNumArgOperands();iop++){
    	tmpDepQbit.clear();
		qGateArg tmpQGateArg;
      	btCount=0;
      	tmpQGateArg.argNum = iop;
      	if(isa<UndefValue>(CI->getArgOperand(iop))){
        	errs() << "WARNING: LLVM IR code has UNDEF values. \n";
       		tmpQGateArg.isUndef = true;   
       	 	//exit(1);
      	}
      	//Checking Inst Types 
      	Type* argType = CI->getArgOperand(iop)->getType();
      	if(argType->isPointerTy()){
        	tmpQGateArg.isPtr = true;
			tmpQGateArg.valOrIndex = 0;
        	Type *argElemType = argType->getPointerElementType();
			int array_size = 0;
    		//if(ArrayType *arrayType = dyn_cast<ArrayType>(argElemType))     
        	//	array_size = arrayType->getNumElements();
        	if(argElemType->isIntegerTy(16)){
          		tmpQGateArg.isQbit = true;
			}
			else if(argElemType->isIntegerTy(8))
				tmpQGateArg.isAbit = true;
			else if(argElemType->isIntegerTy(1))
          		tmpQGateArg.isCbit = true;
      	}
      	else if(argType->isIntegerTy(16)){
        	tmpQGateArg.isQbit = true;
        	tmpQGateArg.valOrIndex = 0;    
      	}               
		else if(argType->isIntegerTy(8)){
			tmpQGateArg.isAbit = true;
			tmpQGateArg.valOrIndex = 0;
		}
      	else if(argType->isIntegerTy(1)){
        	tmpQGateArg.isCbit = true;
        	tmpQGateArg.valOrIndex = 0;    
      	}
		else if(argType->isIntegerTy(32)){
			tmpQGateArg.isCbit = true;
			tmpQGateArg.valOrIndex = dyn_cast<ConstantInt>(CI->getArgOperand(iop))->getZExtValue();
		}
      	//check if argument is constant int
      	if(ConstantInt *CInt = dyn_cast<ConstantInt>(CI->getArgOperand(iop))){
      	  	myPrepState = CInt->getZExtValue();     
      	}
      	//check if argument is constant float
      	if(ConstantFP *CFP = dyn_cast<ConstantFP>(CI->getArgOperand(iop))){
      	  	myRotationAngle = CFP->getValueAPF().convertToDouble();
      	}               
      	if(tmpQGateArg.isQbit || tmpQGateArg.isAbit || tmpQGateArg.isCbit){
      	    tmpDepQbit.push_back(&tmpQGateArg);  
      	    tracked_all_operands &= backtraceOperand(CI->getArgOperand(iop),0);
      	}
      	if(tmpDepQbit.size()>0){          
      	  	allDepQbit.push_back(*(tmpDepQbit[0]));
      	  	assert(tmpDepQbit.size() == 1 && "tmpDepQbit SIZE GT 1");
      	  	tmpDepQbit.clear();
      	}
    } // Finished checking all of the arguments, populated allDepQbits
	if(allDepQbit.size() > 0){
     	if(debugGenSched)
     	{
      	    errs() << "\nCall inst: " << CI->getCalledFunction()->getName();        
      	    errs() << ": Found all arguments: ";       
      	    for(unsigned int vb=0; vb<allDepQbit.size(); vb++){
      	      	if(allDepQbit[vb].argPtr){
      	        	errs() << allDepQbit[vb].argPtr->getName() <<" Index: ";
					errs() << allDepQbit[vb].valOrIndex <<" ";
				}
				else{
      	        	errs() <<"Constant ";
					errs() << allDepQbit[vb].valOrIndex <<" ";
				}
      	    }
      	    errs()<<"\n";
		}
	}
    string fname =  CI->getCalledFunction()->getName();  
    qGate thisGate;
    thisGate.qFunc =  CI->getCalledFunction();

    if(myPrepState!=-1) thisGate.angle = (float)myPrepState;
    if(myRotationAngle!=0.0) thisGate.angle = myRotationAngle;
	if(thisGate.qFunc->getName().find("llvm.") == string::npos){
    	for(unsigned int vb=0; vb<allDepQbit.size(); vb++){
    		if(allDepQbit[vb].argPtr){
    	    	qGateArg param =  allDepQbit[vb];       
				qArgInfo toAdd;
				toAdd.name = param.argPtr->getName();
    	    	if(!param.isPtr) toAdd.index = param.valOrIndex;
				thisGate.args.push_back(toAdd);
    	    	thisGate.numArgs++;
    	     }
			else{
    	    	qGateArg param =  allDepQbit[vb];       
				qArgInfo toAdd;
				toAdd.name = "";
    	    	if(!param.isPtr) toAdd.index = param.valOrIndex;
				thisGate.args.push_back(toAdd);
    	    	thisGate.numArgs++;
			}
    	}
	}
	else{ // We have an intrinsic function -- unpack all array qubits
		  // Assume the constant integer length parameter immediately follows array argument
		for(unsigned int vb=0; vb<allDepQbit.size(); vb++){
    		if(allDepQbit[vb].argPtr){
    	    	qGateArg param =  allDepQbit[vb];       
    	    	if(!param.isPtr){
					qArgInfo toAdd;
					toAdd.index = param.valOrIndex;
					toAdd.name = param.argPtr->getName();
//					thisGate.args[thisGate.numArgs].index = param.valOrIndex;
//    	    		thisGate.args[thisGate.numArgs].name = param.argPtr->getName();
					thisGate.args.push_back(toAdd);
    	    		thisGate.numArgs++;
				}
				else{
					int array_size = allDepQbit[vb+1].valOrIndex;
					for (int i = param.valOrIndex; i < param.valOrIndex + array_size; i++){
						qArgInfo toAdd;
						toAdd.index = i;
						toAdd.name = param.argPtr->getName();
//						thisGate.args[thisGate.numArgs].index = i;
//						thisGate.args[thisGate.numArgs].name = param.argPtr->getName();	
						thisGate.args.push_back(toAdd);
						thisGate.numArgs++;
					}
				}
    	     }
			else{
					/* Omit constants from flowing through
    	    	qGateArg param =  allDepQbit[vb];       
    	    	thisGate.args[thisGate.numArgs].name = "";
    	    	if(!param.isPtr) thisGate.args[thisGate.numArgs].index = param.valOrIndex;
    	    	thisGate.numArgs++;
				*/
			}
    	}

	}
    newOp->gate = thisGate;
    allDepQbit.erase(allDepQbit.begin(),allDepQbit.end());
}
bool GenSched::depsMet(op* newOp, int currentTS){
	for(vector<op*>::iterator it = newOp->in_edges.begin(); it != newOp->in_edges.end(); ++it){
		if((*it)->ts < 0 || (*it)->ts >= currentTS) return false;	
	}
	return true;
}
void GenSched::sched(Function* F){
	for(vector<op*>::iterator it = funcOps.begin(); it != funcOps.end(); ++it){
		int ts = 0;
		if((*it)->in_edges.size() > 0){
			int maxTS = 0;
			for(vector<op*>::iterator mit = (*it)->in_edges.begin(); mit != (*it)->in_edges.end(); ++mit){ 
				if((*mit)->ts > maxTS) maxTS = (*mit)->ts;
			}
			ts = maxTS+1;
		}
		bool scheduled = false;
		string funcName = (*it)->name;
		while(!scheduled){
			if(schedule.size() > ts && (*it)->isIntrinsic && schedule[ts].size() < DATA_CONSTRAINT && schedule[ts].front()->isIntrinsic ){
				parallelism++;
				schedule[ts].push_back(*it);
				(*it)->ts = ts;
				scheduled = true;
			}
			else if(schedule.size() <= ts){
				deque<op*> vect;
				vect.push_front((*it));
				(*it)->ts = ts;
				schedule.push_back(vect);
				scheduled = true;
			}
			ts++;
		}
	}
}
bool GenSched::runOnModule (Module &M) {
	init_gate_names();
	init_gates_as_functions();
	int runtime = 0;
	errs() << "M: $::SIMD_K=" << RES_CONSTRAINT <<"; $::SIMD_D=" << DATA_CONSTRAINT 
	  << "; $::SIMD_L=" << SIMD_L_SCHED << "\n"; 
	// iterate over all functions, and over all instructions in those functions
	CallGraphNode* rootNode = getAnalysis<CallGraph>().getRoot();
	//Post-order
	for (scc_iterator<CallGraphNode*> sccIb = scc_begin(rootNode), E = scc_end(rootNode);
	  sccIb != E; ++sccIb) {
		const std::vector<CallGraphNode*> &nextSCC = *sccIb;
		for (std::vector<CallGraphNode*>::const_iterator nsccI = nextSCC.begin(), E = nextSCC.end();
		  nsccI != E; ++nsccI) {
	    	Function *F = (*nsccI)->getFunction();      
//			vector<qGateArg*> functionArguments;
			vectQbits.clear();
			qubits.clear();
			tmpDepQbit.clear();
			schedule.clear();
			longestPath.clear();
			funcOps.clear();
			op_cnt = 0;
			ts_cnt = 0;
	    	if(F && !F->isDeclaration()){
				parallelism = 0;
				string funcName = F->getName();
				getFunctionArguments(F);
				op_cnt = processInstructions(F);
//				printFuncOps();
//				printDependencyGraph();
				contractAllGateNames();
				sched(F);
				printHeader(F,op_cnt,ts_cnt);
				ts_cnt = printSchedule();
				processSchedule(&M);
				opLengths.insert(make_pair(F,ts_cnt));
				printRuntime(ts_cnt,funcName);
	      	}
	    	else  {
	    		if(debugGenSched) errs() << "WARNING: Ignoring external node or dummy function.\n";
	    	}
		}
	}
	return false;
}
