//===------------------ RuntimeRevMemManagerHybrid.cpp  ------------------===//
//
//                     The LLVM Scaffold Compiler Infrastructure
//
// This file was created by Scaffold Compiler Working Group
//
//===----------------------------------------------------------------------===//

#include <sstream>
#include <iomanip>
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instruction.h"
#include "llvm/Constants.h"
#include "llvm/Intrinsics.h"
#include "llvm/Support/InstVisitor.h" 
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/LLVMContext.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;
using namespace std;

#define _MAX_FUNCTION_NAME 90
#define _MAX_INT_PARAMS 4
#define _MAX_DOUBLE_PARAMS 4
#define _GLOBAL 0
#define _GLOBAL_MAX_SIZE 100000
#define _HIERAR 1
#define _EAGER 0
#define _LAZY 1
#define _OPT 2
#define _NOFREE 3

#define _X 0
#define _Y 1
#define _Z 2
#define _H 3
#define _T 4
#define _Tdag 5
#define _S 6
#define _Sdag 7
#define _CNOT 8
#define _PrepZ 9
#define _MeasZ 10
#define _PrepX 11
#define _MeasX 12
#define _Fredkin 13
#define _Toffoli 14
#define _Rx 15
#define _Ry 16
#define _Rz 17

// Policy switch
//int allocPolicy = _GLOBAL;
int freePolicy = _OPT; // do not change! always opt!!

// DEBUG switch
bool debugRTRevMemHyb = false;

static cl::opt<string>
DEVICENAME("device", cl::init(""), cl::Hidden,
  cl::desc("Location of device description file"));

namespace {

  vector<Instruction*> vInstRemove;

  struct RTRevMemHyb : public ModulePass {

    static char ID;  // Pass identification, replacement for typeid

    //external instrumentation function
    Function* qasmGate; 
    Function* qasmResSum; 
    Function* recordGate; 
    Function* memHeapAlloc; 
    Function* getHeapIdx; 
    Function* memHeapFree; 
		Function* freeOnOff;
    Function* checkAndSched; 
    //Function* memoize; 
    Function* qasmInitialize; 
    Function* exit_scope;

    //uint32_t rep_val;
    Value* rep_val;

    RTRevMemHyb() : ModulePass(ID) {  }

		size_t getNumQubitsByName(std::string origName, unsigned *n_out, unsigned *n_anc, unsigned *n_gate) {
				size_t found_pos_begin = origName.find(std::string("_Release_O_"));
			  //because there might be numbers more than 1 digit long, need to find begin and end
			  size_t found_pos_end = origName.find_first_not_of("012345679-x",found_pos_begin+11);
			  std::string intString = origName.substr(found_pos_begin+11, found_pos_end-(found_pos_begin+11));
			  *n_out = atoi(intString.c_str());
				found_pos_begin = origName.find(std::string("_A_"));
			  //because there might be numbers more than 1 digit long, need to find begin and end
			  found_pos_end = origName.find_first_not_of("012345679-x",found_pos_begin+3);
			  intString = origName.substr(found_pos_begin+3, found_pos_end-(found_pos_begin+3));
			  *n_anc = atoi(intString.c_str());
				found_pos_begin = origName.find(std::string("_G_"));
			  //because there might be numbers more than 1 digit long, need to find begin and end
			  found_pos_end = origName.find_first_not_of("012345679-x",found_pos_begin+3);
			  intString = origName.substr(found_pos_begin+3, found_pos_end-(found_pos_begin+3));
			  *n_gate = atoi(intString.c_str());
				return found_pos_end; // start of callerName
		}
		
		std::string mangleType(vector<Type*> arg_types, std::string orig) {
			stringstream ss;
			int i = 0;
			for (vector<Type*>::iterator it = arg_types.begin(); it != arg_types.end(); ++it) {
				if (orig == "memHeapAlloc" && i == 3) {
					ss << "S0_";
					i++;
					continue;
				}
				if ((*it)->isPointerTy()) {
					vector<Type*> new_t;
					new_t.push_back((*it)->getPointerElementType());
					ss << 'P' << mangleType(new_t, orig);
				} else {
					//switch ((*it)->getKind()) {
  				//	case BuiltinType::Void: ss << 'v'; break;
  				//	case BuiltinType::Bool: ss << 'b'; break;
  				//	case BuiltinType::Char_U: case BuiltinType::Char_S: ss << 'c'; break;
  				//	case BuiltinType::UChar: ss << 'h'; break;
  				//	case BuiltinType::UShort: ss << 't'; break;
  				//	case BuiltinType::UInt: ss << 'j'; break;
  				//	case BuiltinType::ULong: ss << 'm'; break;
  				//	case BuiltinType::ULongLong: ss << 'y'; break;
  				//	case BuiltinType::UInt128: ss << 'o'; break;
  				//	case BuiltinType::SChar: ss << 'a'; break;
  				//	case BuiltinType::WChar_S:
  				//	case BuiltinType::WChar_U: ss << 'w'; break;
  				//	case BuiltinType::Char16: ss << "Ds"; break;
  				//	case BuiltinType::Char32: ss << "Di"; break;
  				//	case BuiltinType::Short: ss << 's'; break;
  				//	case BuiltinType::Int: ss << 'i'; break;
  				//	case BuiltinType::Long: ss << 'l'; break;
  				//	case BuiltinType::LongLong: ss << 'x'; break;
  				//	case BuiltinType::Int128: ss << 'n'; break;
  				//	case BuiltinType::Half: ss << "Dh"; break;
  				//	case BuiltinType::Float: ss << 'f'; break;
  				//	case BuiltinType::Double: ss << 'd'; break;
  				//	case BuiltinType::LongDouble: ss << 'e'; break;
  				//	case BuiltinType::NullPtr: ss << "Dn"; break;
  				//	case BuiltinType::Abit: ss << 'a'; break; // Scaffold
  				//	case BuiltinType::Cbit: ss << "Cb"; break;  // Scaffold
  				//	case BuiltinType::Qbit: ss << 'q'; break;  // Scaffold
  				//	case BuiltinType::Qint: ss << 'y'; break;  // Scaffold
					//}
					if ((*it)->isVoidTy()) ss << 'v'; 
					if ((*it)->isIntegerTy(16)) ss << 's'; 
					if ((*it)->isIntegerTy(32)) ss << 'i'; 
				}
			i++;
			}
			return ss.str();
		}

		std::string getMangleName(std::string orig, vector<Type*> arg_types) {
			
			stringstream ss;
			ss << "_Z" << orig.length() << orig;
			ss << mangleType(arg_types, orig);
			return ss.str();
		}

		TerminatorInst *addIfThen(Value *Cond, Instruction *SplitBefore,
									 BasicBlock *nextB,
                                bool Unreachable, BasicBlock::iterator IT, 
																MDNode *BranchWeights=0,
                                DominatorTree *DT=0, LoopInfo *LI=0) {
		  BasicBlock *Head = SplitBefore->getParent();
		  //BasicBlock *Tail = Head->splitBasicBlock(SplitBefore->getIterator());
		  BasicBlock *Tail = Head->splitBasicBlock(IT);
		  TerminatorInst *HeadOldTerm = Head->getTerminator();
		  //LLVMContext &C = Head->getContext();
		  //BasicBlock *ThenBlock = BasicBlock::Create(C, "", Head->getParent(), Tail);
		  //TerminatorInst *CheckTerm;
		  //if (Unreachable)
		  //  CheckTerm = new UnreachableInst(C, ThenBlock);
		  //else
		  //  CheckTerm = BranchInst::Create(Tail, ThenBlock);
		  //CheckTerm->setDebugLoc(SplitBefore->getDebugLoc());
		  BranchInst *HeadNewTerm =
		    BranchInst::Create(/*ifTrue*/Tail, /*ifFalse*/nextB, Cond);
		  //HeadNewTerm->setMetadata(LLVMContext::MD_prof, BranchWeights);
		  ReplaceInstWithInst(HeadOldTerm, HeadNewTerm);
		
		  //if (DT) {
		  //  if (DomTreeNode *OldNode = DT->getNode(Head)) {
		  //    std::vector<DomTreeNode *> Children(OldNode->begin(), OldNode->end());
		
		  //    DomTreeNode *NewNode = DT->addNewBlock(Tail, Head);
		  //    for (DomTreeNode *Child : Children)
		  //      DT->changeImmediateDominator(Child, NewNode);
		
		  //    // Head dominates ThenBlock.
		  //    //DT->addNewBlock(ThenBlock, Head);
		  //  }
		  //}
		
		  //if (LI) {
		  //  if (Loop *L = LI->getLoopFor(Head)) {
		  //    //L->addBasicBlockToLoop(ThenBlock, (*LI).getBase());
		  //    L->addBasicBlockToLoop(Tail, (*LI).getBase());
		  //  }
		  //}
		
		  // return CheckTerm;
		  return Tail->getTerminator();
		}

		void declare2memHeapFree(Function *CF, CallInst *CI, BasicBlock::iterator I) {
			unsigned num_qbits = 0; // num of anc that's free
			unsigned num_outs= 0; // num of out bits that's copied
			unsigned num_gates= 0; // num of gates copied
			unsigned total_narg = CI->getNumArgOperands();
			// assume declare_free(anc, nAnc)
			if (total_narg != 2) {
				errs() << "Invalid declare_free function encountered: numArg = " << total_narg << " != 2.\n";
			}
			unsigned nAnc_idx = 1;
			ConstantInt *nQ = dyn_cast<ConstantInt>(CI->getArgOperand(nAnc_idx));
			//if (nQ == NULL) {
			//	// nAnc is undef
			//	if(debugRTRevMemHyb)
			//		errs() << "\tRelease instruction: " << *CI << "\n";
			//	std::string origName = CF->getName(); 
			//	getNumQubitsByName(origName, &num_outs, &num_qbits, &num_gates);
			//  nQ = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),num_qbits);
			//} else {
			//	num_qbits = nQ->getZExtValue(); // number of qubits to free
			//}
			num_qbits = nQ->getZExtValue(); // number of qubits to free
			if(debugRTRevMemHyb)
			  errs() << "\tPotential free: " << num_qbits << " qubits.\n";
			// Get the heap idx
			CallInst *hidx = CallInst::Create(getHeapIdx, "", CI);
			//CallInst *hidx = CallInst::Create(getHeapIdx, "", (Instruction *)CI);
			// Get the stack array for results
			Value *res = CI->getArgOperand(nAnc_idx-1);
			// Create the memHeapFree call
			vector<Value*> freeArgs;
			freeArgs.push_back(nQ);
			freeArgs.push_back(hidx);
			freeArgs.push_back(res);
			CallInst::Create(memHeapFree, ArrayRef<Value*>(freeArgs), "", CI);
		
			errs() << "\tPotential free: " << num_qbits << " qubits.\n";
			//if(delAfterInst)
			//  vInstRemove.push_back((Instruction*)CI);

		}

		void translateFree(Function *CF, CallInst *CI, BasicBlock::iterator I) {
			unsigned total_narg = CI->getNumArgOperands();
			// assume _free_option(out, nout, anc, nAnc, ngate)
			if (total_narg != 5) {
				errs() << "Invalid Free() encountered: numArg = " << total_narg << " != 5.\n";
			}
			vector<Value*> onoffArgs;
			onoffArgs.push_back(CI->getArgOperand(1)); //nout
			onoffArgs.push_back(CI->getArgOperand(3)); //nanc
			onoffArgs.push_back(CI->getArgOperand(4)); //ngate
			onoffArgs.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()),0));// flag
			CallInst *onoff = CallInst::Create(freeOnOff, ArrayRef<Value*>(onoffArgs), "", CI);
			CI->replaceAllUsesWith(onoff);
		  //ReplaceInstWithInst(CI, onoff);
			 vInstRemove.push_back((Instruction*)CI);
	
			//return onoff;
		}

		void release2memHeapFree(Function *CF, CallInst *CI, BasicBlock::iterator I, Instruction *Bterm) {
			unsigned num_qbits = 0; // num of anc that's free
			unsigned num_outs= 0; // num of out bits that's copied
			unsigned num_gates= 0; // num of gates copied
			unsigned total_narg = CI->getNumArgOperands();
			// assume release(x,...,x, out, nOut, anc, nAnc, cpy, alloc, nAlloc)
			if (total_narg < 7) {
				errs() << "Invalid Release function encountered: numArg = " << total_narg << " < 7.\n";
			}
			unsigned nAnc_idx = total_narg - 4; // or total_narg - 1
			ConstantInt *nQ = dyn_cast<ConstantInt>(CI->getArgOperand(nAnc_idx));
			if (nQ == NULL) {
				// nAnc is undef
				if(debugRTRevMemHyb)
					errs() << "\tRelease instruction: " << *CI << "\n";
				std::string origName = CF->getName(); 
				getNumQubitsByName(origName, &num_outs, &num_qbits, &num_gates);
			  nQ = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),num_qbits);
			} else {
				num_qbits = nQ->getZExtValue(); // number of qubits to free
			}
			if(debugRTRevMemHyb)
			  errs() << "\tPotential free: " << num_qbits << " qubits.\n";
			// Get the heap idx
			CallInst *hidx = CallInst::Create(getHeapIdx, "", Bterm);
			//CallInst *hidx = CallInst::Create(getHeapIdx, "", (Instruction *)CI);
			// Get the stack array for results
			Value *res = CI->getArgOperand(nAnc_idx-1);
			// Create the memHeapFree call
			vector<Value*> freeArgs;
			freeArgs.push_back(nQ);
			freeArgs.push_back(hidx);
			freeArgs.push_back(res);
			CallInst::Create(memHeapFree, ArrayRef<Value*>(freeArgs), "", Bterm);
		
			//errs() << "\tPotential free: " << num_qbits << " qubits.\n";
			//if(delAfterInst)
			//  vInstRemove.push_back((Instruction*)CI);

		}

		void release2Lazy(Function *CF, CallInst *CI, BasicBlock::iterator I) {

		}

		BasicBlock::iterator release2Opt(Function *CF, CallInst *CI, BasicBlock::iterator I, BasicBlock *BB, Function *PF, BasicBlock *nextB) {
			unsigned num_ancs = 0; // num of anc that's free
			unsigned num_outs= 0; // num of out bits that's copied
			unsigned num_gates= 0; // num of gates copied
			std::string origName = CF->getName(); 
			//size_t pos_caller = getNumQubitsByName(origName, &num_outs, &num_ancs, &num_gates);
			vector<Value*> onoffArgs;
			onoffArgs.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()),num_outs));
			onoffArgs.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()),num_ancs));
			ConstantInt *n_gates = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),num_gates);
			//onoffArgs.push_back(n_gates);
			onoffArgs.push_back(n_gates);
			onoffArgs.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()),freePolicy));
			CallInst *onoff = CallInst::Create(freeOnOff, ArrayRef<Value*>(onoffArgs), "", CI);
			// create predicate
			Value *pred = new ICmpInst(CI, CmpInst::ICMP_EQ, onoff, ConstantInt::get(Type::getInt32Ty(getGlobalContext()),1), "");
			//Instruction *temp;
			
			//TerminatorInst *termInst = SplitBlockAndInsertIfThen(pred, CI, false, I);
			BasicBlock::iterator nextI = I;
			nextI++;
			nextB = BB->splitBasicBlock(nextI, BB->getName()+".split");
			TerminatorInst *termInst = addIfThen(pred, CI, nextB, false, I);

			release2memHeapFree(CF, CI, I, termInst);

			//vInstRemove.push_back((Instruction*)CI);

			BasicBlock::iterator bi(termInst);
			return bi;
			//return 0;
		}

		void release2Eager(Function *CF, CallInst *CI, BasicBlock::iterator I, BasicBlock *BB) {
			release2memHeapFree(CF, CI, I, BB->getTerminator());
		}

    void visitCallInst (BasicBlock::iterator I, AllocaInst* strAlloc, AllocaInst* intArrAlloc, AllocaInst* doubleArrAlloc, BasicBlock *BB, Function *PF, bool *releaseflag, Function::iterator nextB) {
      CallInst *CI = dyn_cast<CallInst>(&*I);

      Function *CF = CI->getCalledFunction();

      if(debugRTRevMemHyb)
        errs() << "\tCalls: " << CF->getName() << "\n";

      int gateIndex = 0;
      bool isIntrinsicQuantum = true;
      bool delAfterInst = true; // do not delete Meas gates because it will invalidate cbit stores

      bool isQuantumModuleCall = false;

      // is this a call to a quantum module? Only those should be instrumented
      // quantum modules arguments are either qbit or qbit* type
      for(unsigned iop=0;iop < CI->getNumArgOperands(); iop++) {
        //errs() << "\t iop" << iop << "/" << CI->getNumArgOperands()<< "\n";
        if (CI->getArgOperand(iop)->getType()->isPointerTy()) {
          if (CI->getArgOperand(iop)->getType()->getPointerElementType()->isIntegerTy(16)) {
            isQuantumModuleCall = true; // take a qubit (i16*)
					} else if (CI->getArgOperand(iop)->getType()->getPointerElementType()->isPointerTy()) {
						if (CI->getArgOperand(iop)->getType()->getPointerElementType()->getPointerElementType()->isIntegerTy(16)) {
            	isQuantumModuleCall = true; // take an array of qubits (i16**)
						}
					}
        } else if (CI->getArgOperand(iop)->getType()->isIntegerTy(16)) {
          isQuantumModuleCall = true; // old definition
				}
      }
     	 
      if(debugRTRevMemHyb)
        errs() << "\t takes qubits? " << isQuantumModuleCall << "\n";

      if(CF->getName().find("store_cbit") != std::string::npos)
        vInstRemove.push_back((Instruction*)CI);
      //errs() << "Is intrinsic? " << CF->isIntrinsic() << "\n";
      if(CF->isIntrinsic()) {
				//errs() << "now in!!!: " << CF->getIntrinsicID() << " " << Intrinsic::CNOT << " " << Intrinsic::Y << "\n";
        if(CF->getIntrinsicID() == Intrinsic::CNOT) gateIndex = _CNOT;
        else if(CF->getIntrinsicID() == Intrinsic::Fredkin) gateIndex = _Fredkin;
        else if(CF->getIntrinsicID() == Intrinsic::H) gateIndex = _H;
        else if(CF->getIntrinsicID() == Intrinsic::MeasX) { gateIndex = _MeasX; delAfterInst = false; }
        else if(CF->getIntrinsicID() == Intrinsic::MeasZ) { gateIndex = _MeasZ; delAfterInst = false; }
        else if(CF->getIntrinsicID() == Intrinsic::PrepX) gateIndex = _PrepX;
        else if(CF->getIntrinsicID() == Intrinsic::PrepZ) gateIndex = _PrepZ;
        else if(CF->getIntrinsicID() == Intrinsic::Rx) gateIndex = _Rx;
        else if(CF->getIntrinsicID() == Intrinsic::Ry) gateIndex = _Ry;
        else if(CF->getIntrinsicID() == Intrinsic::Rz) gateIndex = _Rz;
        else if(CF->getIntrinsicID() == Intrinsic::S) gateIndex = _S;
        else if(CF->getIntrinsicID() == Intrinsic::T) gateIndex = _T;
        else if(CF->getIntrinsicID() == Intrinsic::Sdag) gateIndex = _Sdag;
        else if(CF->getIntrinsicID() == Intrinsic::Tdag) gateIndex = _Tdag;
        else if(CF->getIntrinsicID() == Intrinsic::Toffoli) gateIndex = _Toffoli;
        else if(CF->getIntrinsicID() == Intrinsic::X) gateIndex = _X;
        else if(CF->getIntrinsicID() == Intrinsic::Y) gateIndex = _Y;
        else if(CF->getIntrinsicID() == Intrinsic::Z) gateIndex = _Z;
        else { isIntrinsicQuantum = false; delAfterInst = false; }
        if (isIntrinsicQuantum) {
          //vector <Value*> vectCallArgs;
					//errs() << "now in: << " << CF->getName() << "\n";
          Constant* gateID = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), gateIndex, false);	
          //Constant* RepeatConstant = ConstantInt::get(Type::getInt32Ty(getGlobalContext()) , rep_val, false);
          
					//errs() << "now in!\n";
					vector<Value*> gateIDArg;
					gateIDArg.push_back(gateID);

					unsigned nArg = CI->getNumArgOperands();
					
        	ArrayType *qArrTy = ArrayType::get(Type::getInt16Ty(getGlobalContext())->getPointerTo(), nArg);
        	AllocaInst *qArrAlloc = new AllocaInst(qArrTy, "", (Instruction*)CI);
					Value *Idx[2];
        	Idx[0] = Constant::getNullValue(Type::getInt32Ty(CI->getContext()));  
					for (size_t i = 0; i < nArg; i++) {
						Value *aa = CI->getArgOperand(nArg-i-1); //WHAT!
						//gateIDArg.push_back(aa);
						Idx[1] = ConstantInt::get(Type::getInt32Ty(CI->getContext()),nArg-i-1);        
          	Value *qPtr = GetElementPtrInst::CreateInBounds(qArrAlloc, Idx, "", (Instruction*)CI);        
          	new StoreInst(aa, qPtr, "", (Instruction*)CI);
					}
        	GetElementPtrInst* qArrPtr = GetElementPtrInst::CreateInBounds(qArrAlloc, Idx, "", (Instruction*)CI);
					gateIDArg.push_back(qArrPtr);
					ConstantInt *nArgV = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), nArg, false);
					
					gateIDArg.push_back(nArgV);
					//errs() << "now in!\n";
					CallInst::Create(checkAndSched, ArrayRef<Value*>(gateIDArg), "", (Instruction *)CI);
          //vectCallArgs.push_back(gateID);
          //vectCallArgs.push_back(RepeatConstant);
          //vectCallArgs.push_back(rep_val);       
          
          //ArrayRef<Value*> call_args(vectCallArgs);  
          
          //CallInst::Create(qasmGate, "", (Instruction*)CI);
          
					//errs() << "now in!\n";
          if(delAfterInst)
            vInstRemove.push_back((Instruction*)CI);

        }
      }
			else { // not intrinsic

				//errs() << "calling: << " << CF->getName() << "\n";

				if (CF->getName().find("acquire") != std::string::npos) {
					//errs() << "Found acquire\n";
					// repalce with memHeapAlloc
					unsigned num_qbits = 0;
					ConstantInt *nQ = dyn_cast<ConstantInt>(CI->getArgOperand(0));
					num_qbits = nQ->getZExtValue(); // number of qubits to allocate
					
    	    if(debugRTRevMemHyb)
    	      errs() << "\tRequesting for: " << num_qbits << " qubits.\n";
					// Get the heap idx
					CallInst *hidx = CallInst::Create(getHeapIdx, "", (Instruction *)CI);
					// Get the stack array for results: acquire(num_qbits, array, ninter, inter_array)
					Value *res = CI->getArgOperand(1);
					// Create the memHeapAlloc call
					vector<Value*> allocArgs;
					allocArgs.push_back(nQ);
					allocArgs.push_back(hidx);
					allocArgs.push_back(res);
					Value *inter = CI->getArgOperand(3); // qbit_t **interaction
					allocArgs.push_back(inter);
					Value *ninter = CI->getArgOperand(2); // int num_interaction
					allocArgs.push_back(ninter);
					CallInst::Create(memHeapAlloc, ArrayRef<Value*>(allocArgs), "", (Instruction *)CI);
					//errs() << "pass\n";

    	    if(delAfterInst)
    	      vInstRemove.push_back((Instruction*)CI);
				}      
 				else if (CF->getName().find("_Release_O_") != std::string::npos) {
					//errs() << "BB size: " << BB->size() << "\n";
					if (freePolicy == _NOFREE) { 
						vInstRemove.push_back((Instruction*)CI);
					} else if (freePolicy == _LAZY) {
						// TODO: only replace the top level releases with memHeapFree
						release2Lazy(CF, CI, I);
					} else if (freePolicy == _OPT) {
						// TODO: optionally replace the releases with memHeapFree
						//if (BB->size() > 115) {
						CallInst *CIp = dyn_cast<CallInst>(&*I);
						//errs() << "I: " << I << " :: " << *CIp << "\n";
						release2Opt(CF, CI, I, BB, PF, nextB);
						// change the iterator to avoid infinite loop
						// *newI = bi;
						//*newI = (PF->getBasicBlockList()).end(); 
						*releaseflag = true;
						CIp = dyn_cast<CallInst>(&*I);
						//errs() << "I: " << I << " :: " << *CIp << "\n";
						//I++;
						//CIp = dyn_cast<CallInst>(&*I);
						//errs() << "I++: " << I << " :: " << *I << "\n";
						//}
					} else { // _EAGER
						// repalce with memHeapFree
						release2Eager(CF, CI, I, BB);
    	    }
					
				}      
				else if (CF->getName().find("_free_option") != std::string::npos) {
					//errs() << "Found _free_option\n";
					translateFree(CF, CI, I);//add a memHeapFree after the inst
					//errs() << "Complete _free_option\n";
				}
				else if (CF->getName().find("declare_free") != std::string::npos) {
					//errs() << "Found declare_free\n";
					declare2memHeapFree(CF, CI, I);//add a memHeapFree after the inst
				}
    	  else if (!CF->isDeclaration() && isQuantumModuleCall){
					//errs() << "no declare  visit\n";
					//visitFunction(*CF);
    	    // insert memoize call before this function call
    	    // int memoize ( char *function_name, int *int_params, unsigned num_ints, double *double_params, unsigned num_doubles, unsigned repeat)          
    	    
    	    //vector <Value*> vectCallArgs;
    	    //
    	    //std::stringstream ss;
    	    //ss << std::left << std::setw (_MAX_FUNCTION_NAME-1) << std::setfill(' ') << CF->getName().str();
    	    //Constant *StrConstant = ConstantDataArray::getString(CI->getContext(), ss.str());                   
    	    //
    	    //new StoreInst(StrConstant,strAlloc,"",(Instruction*)CI);	  	  
    	    //Value* Idx[2];	  
    	    //Idx[0] = Constant::getNullValue(Type::getInt32Ty(CI->getContext()));  
    	    //Idx[1] = ConstantInt::get(Type::getInt32Ty(CI->getContext()),0);
    	    //GetElementPtrInst* strPtr = GetElementPtrInst::Create(strAlloc, Idx, "", (Instruction*)CI);
    	    //
    	    //Value *intArgPtr;
    	    //vector<Value*> vIntArgs;
    	    //unsigned num_ints = 0;
    	    //Value *doubleArgPtr;
    	    //vector<Value*> vDoubleArgs;
    	    //unsigned num_doubles = 0;

    	    //for(unsigned iop=0; iop < CI->getNumArgOperands(); iop++) {
    	    //  Value *callArg = CI->getArgOperand(iop);
    	    //  // Integer Arguments
    	    //  if(ConstantInt *CInt = dyn_cast<ConstantInt>(callArg)){
    	    //    intArgPtr = CInt;
    	    //    num_ints++;
    	    //    vIntArgs.push_back(intArgPtr);          
    	    //  }
    	    //  else if (callArg->getType() == Type::getInt32Ty(CI->getContext())){ //FIXME: make sure it's an integer
    	    //    intArgPtr = CastInst::CreateIntegerCast(callArg, Type::getInt32Ty(CI->getContext()), false, "", (Instruction*)CI);
    	    //    num_ints++;
    	    //    vIntArgs.push_back(intArgPtr);          
    	    //  }

    	    //  // Double Arguments
    	    //  if(ConstantFP *CDouble = dyn_cast<ConstantFP>(CI->getArgOperand(iop))){ 
    	    //    doubleArgPtr = CDouble;
    	    //    vDoubleArgs.push_back(doubleArgPtr);          
    	    //    num_doubles++;
    	    //  }
    	    //  else if (callArg->getType() == Type::getDoubleTy(CI->getContext())){ //FIXME: make sure it's an integer
    	    //    doubleArgPtr = CastInst::CreateFPCast(callArg, Type::getDoubleTy(CI->getContext()), "", (Instruction*)CI);          
    	    //    num_doubles++;
    	    //    vDoubleArgs.push_back(doubleArgPtr);          
    	    //  }
    	    //}
    	    //
    	    //for (unsigned i=0; i<num_ints; i++) {
    	    //  Value *Int = vIntArgs[i];        
    	    //  Idx[1] = ConstantInt::get(Type::getInt32Ty(CI->getContext()),i);        
    	    //  Value *intPtr = GetElementPtrInst::CreateInBounds(intArrAlloc, Idx, "", (Instruction*)CI);        
    	    //  new StoreInst(Int, intPtr, "", (Instruction*)CI);
    	    //}
    	    //Idx[1] = ConstantInt::get(Type::getInt32Ty(CI->getContext()),0);        
    	    //GetElementPtrInst* intArrPtr = GetElementPtrInst::CreateInBounds(intArrAlloc, Idx, "", (Instruction*)CI);

    	    //for (unsigned i=0; i<num_doubles; i++) {
    	    //  Value *Double = vDoubleArgs[i];     
    	    //  Idx[1] = ConstantInt::get(Type::getInt32Ty(CI->getContext()),i);        
    	    //  Value *doublePtr = GetElementPtrInst::CreateInBounds(doubleArrAlloc, Idx, "", (Instruction*)CI);        
    	    //  new StoreInst(Double, doublePtr, "", (Instruction*)CI);          
    	    //}
    	    //GetElementPtrInst* doubleArrPtr = GetElementPtrInst::CreateInBounds(doubleArrAlloc, Idx, "", (Instruction*)CI);

    	    //Constant *IntNumConstant = ConstantInt::get(Type::getInt32Ty(getGlobalContext()) , num_ints, false);       
    	    //Constant *DoubleNumConstant = ConstantInt::get(Type::getInt32Ty(getGlobalContext()) , num_doubles, false);          

    	    ////Constant *RepeatConstant = ConstantInt::get(Type::getInt32Ty(getGlobalContext()) , rep_val, false);

    	    ////vectCallArgs.push_back(cast<Value>(strPtr));
    	    ////vectCallArgs.push_back(cast<Value>(intArrPtr));
    	    ////vectCallArgs.push_back(IntNumConstant);          
    	    ////vectCallArgs.push_back(cast<Value>(doubleArrPtr));
    	    ////vectCallArgs.push_back(DoubleNumConstant);          
    	    //////vectCallArgs.push_back(RepeatConstant);       
    	    ////vectCallArgs.push_back(rep_val);       

    	    ////ArrayRef<Value*> call_args(vectCallArgs);  
    	    //
    	    ////CallInst::Create(memoize, call_args, "", (Instruction*)CI);      

    	    ////CallInst::Create(memoize, getMemoizeArgs(CI, strAlloc, intArrAlloc, doubleArrAlloc), "", (Instruction*)CI);
    	    //
    	    ////vector <Value*> vectCallArgs2;              
    	    ////vectCallArgs2.push_back(rep_val);       
    	    ////ArrayRef<Value*> call_args2(vectCallArgs2);   

    	    //CallInst::Create(exit_scope, "", (&*++I));
    	    
    	    isIntrinsicQuantum = false;
    	    delAfterInst = false;
    	  }

    	  else if (CF->getName().find("qasmRepLoopStart") != string::npos) {
    	    rep_val = CI->getArgOperand(0);
    	    vInstRemove.push_back((Instruction*)CI);
    	  }
    	  
    	  else if (CF->getName().find("qasmRepLoopEnd") != string::npos) {
    	    rep_val = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1, false);
    	    vInstRemove.push_back((Instruction*)CI);        
    	  }           

			}
      
    }
    
    void visitFunction(Function &F) {
      // insert alloca instructions at the beginning for subsequent memoize calls   
      bool isQuantumModule = false;
      for(Function::arg_iterator ait=F.arg_begin();ait!=F.arg_end();++ait) {
        if (ait->getType()->isPointerTy()) {

          if(ait->getType()->getPointerElementType()->isIntegerTy(16)) {
            isQuantumModule = true;
					} else if (ait->getType()->getPointerElementType()->isPointerTy()) {
						// check if is of type: qbit **
          	if(ait->getType()->getPointerElementType()->getPointerElementType()->isIntegerTy(16)) {
            	isQuantumModule = true;
						}
					}
        } else if (ait->getType()->isIntegerTy(16)) {
					// intrinsics operates on i16, consider changing later
          isQuantumModule = true;
				}   
      }
      if (F.getName() == "main") 
          isQuantumModule = true;      
      if(!F.isDeclaration() && isQuantumModule){
        BasicBlock* BB_first = &(F.front());
        BasicBlock::iterator BBiter = BB_first->getFirstNonPHI();
        while(isa<AllocaInst>(BBiter))
          ++BBiter;
        Instruction* pInst = &(*BBiter);
  
        ArrayType *strTy = ArrayType::get(Type::getInt8Ty(pInst->getContext()), _MAX_FUNCTION_NAME);
        AllocaInst *strAlloc = new AllocaInst(strTy,"",pInst);
        
        ArrayType *intArrTy = ArrayType::get(Type::getInt32Ty(pInst->getContext()), _MAX_INT_PARAMS);
        AllocaInst *intArrAlloc = new AllocaInst(intArrTy, "", pInst);

        ArrayType *doubleArrTy = ArrayType::get(Type::getDoubleTy(pInst->getContext()), _MAX_DOUBLE_PARAMS);        
        AllocaInst *doubleArrAlloc = new AllocaInst(doubleArrTy,"",pInst);

        if(debugRTRevMemHyb)
          errs() << "Function: " << F.getName() << "\n";
				bool releaseflag = false;
        for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
					//errs() << "F size: " << F.size() << "\n";
        	if(debugRTRevMemHyb)
						errs() << "new block with size: " << BB->size() << " \n";
					Function::iterator nextB = BB;
					nextB++;
					//nextB++;
          for (BasicBlock::iterator I = (*BB).begin(); I != (*BB).end(); ++I) {
            if (dyn_cast<CallInst>(&*I)) {
							//errs() << "outer I: " << I << " :: " << *I << "\n";
							//errs() << "Visiting...\n";
              visitCallInst(I, strAlloc, intArrAlloc, doubleArrAlloc, BB, &F, &releaseflag, nextB);
							//errs() << "Done visiting...: " << releaseflag<< "\n";
							if (releaseflag) break;

						}
          }
					if (nextB == F.end()) {
						break;
					}
					if (releaseflag) {
						BB = nextB;
					}
        }
      }
    }
    
    bool runOnModule(Module &M) {
      //rep_val = 1;  
      rep_val = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1, false);

      // void exit_scope ()
      vector<Type*> esArgTypes;
			esArgTypes.push_back(Type::getVoidTy(M.getContext()));
      std::string exit_scope_str = getMangleName("exit_scope", esArgTypes);
			//errs() << exit_scope_str << "\n";
      exit_scope = cast<Function>(M.getOrInsertFunction(exit_scope_str, Type::getVoidTy(M.getContext()), (Type*)0));

      //void initialize ()
      qasmInitialize = cast<Function>(M.getOrInsertFunction(getMangleName("qasm_initialize",esArgTypes), Type::getVoidTy(M.getContext()), (Type*)0));
      
      //void qasm_resource_summary ()
      qasmResSum = cast<Function>(M.getOrInsertFunction(getMangleName("qasm_resource_summary",esArgTypes), Type::getVoidTy(M.getContext()), (Type*)0));

      // void qasmGate ()      
      qasmGate = cast<Function>(M.getOrInsertFunction(getMangleName("qasm_gate",esArgTypes), Type::getVoidTy(M.getContext()), (Type*)0));      

			// void recordGate(unsigned)
			vector <Type*> rgArgTypes;
      rgArgTypes.push_back(Type::getInt32Ty(M.getContext())); //gateID
      rgArgTypes.push_back(Type::getInt16Ty(M.getContext())->getPointerTo()->getPointerTo());// operands
      rgArgTypes.push_back(Type::getInt32Ty(M.getContext())); //numArgs
      rgArgTypes.push_back(Type::getInt32Ty(M.getContext())); //timestep
      Type* rgResType = Type::getVoidTy(M.getContext());
			recordGate = cast<Function>(M.getOrInsertFunction(getMangleName("recordGate", rgArgTypes), FunctionType::get(rgResType, ArrayRef<Type*>(rgArgTypes), false)));

			// unsigned memHeapAlloc(unsigned, unsigned, qbit **)
			vector <Type*> allocArgTypes;
      allocArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      allocArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      allocArgTypes.push_back(Type::getInt16Ty(M.getContext())->getPointerTo()->getPointerTo());
      allocArgTypes.push_back(Type::getInt16Ty(M.getContext())->getPointerTo()->getPointerTo());
      allocArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      Type* allocResType = Type::getInt32Ty(M.getContext());
			memHeapAlloc = cast<Function>(M.getOrInsertFunction(getMangleName("memHeapAlloc", allocArgTypes), FunctionType::get(allocResType, ArrayRef<Type*>(allocArgTypes), false)));
			
			// unsigned getHeapIdx()
			Type* hidxType = Type::getInt32Ty(M.getContext());
      getHeapIdx = cast<Function>(M.getOrInsertFunction(getMangleName("getHeapIdx", esArgTypes), hidxType, (Type*)0));      

			// unsigned memHeapFree(unsigned, unsigned, qbit **)
			vector <Type*> freeArgTypes;
      freeArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      freeArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      freeArgTypes.push_back(Type::getInt16Ty(M.getContext())->getPointerTo()->getPointerTo());
      Type* freeResType = Type::getInt32Ty(M.getContext());
			memHeapFree = cast<Function>(M.getOrInsertFunction(getMangleName("memHeapFree", freeArgTypes), FunctionType::get(freeResType, ArrayRef<Type*>(freeArgTypes), false)));
	
			// unsigned freeOnOff(unsigned, unsigned, qbit **)
			vector <Type*> onoffArgTypes;
      onoffArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      onoffArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      onoffArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      onoffArgTypes.push_back(Type::getInt32Ty(M.getContext()));
      Type* onoffResType = Type::getInt32Ty(M.getContext());
			freeOnOff = cast<Function>(M.getOrInsertFunction(getMangleName("freeOnOff", onoffArgTypes), FunctionType::get(onoffResType, ArrayRef<Type*>(onoffArgTypes), false)));
	

			// void checkAndSched(unsigned)
			vector <Type*> csArgTypes;
      csArgTypes.push_back(Type::getInt32Ty(M.getContext())); //gateID
      csArgTypes.push_back(Type::getInt16Ty(M.getContext())->getPointerTo()->getPointerTo());// operands
      csArgTypes.push_back(Type::getInt32Ty(M.getContext())); //numArgs
      Type* csResType = Type::getVoidTy(M.getContext());
			checkAndSched = cast<Function>(M.getOrInsertFunction(getMangleName("checkAndSched", csArgTypes), FunctionType::get(csResType, ArrayRef<Type*>(csArgTypes), false)));


      // int memoize (char*, int*, unsigned, double*, unsigned, unsigned)
      //vector <Type*> vectParamTypes2;
      //vectParamTypes2.push_back(Type::getInt8Ty(M.getContext())->getPointerTo());      
      //vectParamTypes2.push_back(Type::getInt32Ty(M.getContext())->getPointerTo());
      //vectParamTypes2.push_back(Type::getInt32Ty(M.getContext()));
      //vectParamTypes2.push_back(Type::getDoubleTy(M.getContext())->getPointerTo());
      //vectParamTypes2.push_back(Type::getInt32Ty(M.getContext()));
      //vectParamTypes2.push_back(Type::getInt32Ty(M.getContext()));
      //ArrayRef<Type*> Param_Types2(vectParamTypes2);
      //Type* Result_Type2 = Type::getInt32Ty(M.getContext());
      //memoize = cast<Function> (  
      //    M.getOrInsertFunction(
      //      "memoize",                          /* Name of Function */
      //      FunctionType::get(                  /* Type of Function */
      //        Result_Type2,                     /* Result */
      //        Param_Types2,                     /* Params */
      //        false                             /* isVarArg */
      //        )
      //      )
      //    );

      // iterate over instructions to instrument the initialize and exit scope calls
      // insert alloca instructions at the beginning for subsequent memoize calls         
      for (Module::iterator F = M.begin(); F != M.end(); ++F) {
        visitFunction(*F); // instrumenting each function
      }      

      // insert initialization and termination functions in "main"
      Function* F = M.getFunction("main");
      if(F){
        BasicBlock* BB_last = &(F->back());
        TerminatorInst *BBTerm = BB_last->getTerminator();
        CallInst::Create(qasmResSum, "",(Instruction*)BBTerm);	

        BasicBlock* BB_first = &(F->front());
        BasicBlock::iterator BBiter = BB_first->getFirstNonPHI();
        while(isa<AllocaInst>(BBiter))
          ++BBiter;
        CallInst::Create(qasmInitialize, "", (Instruction*)&(*BBiter));
      }


      // removing instructions that were marked for deletion
      for(vector<Instruction*>::iterator iterInst = vInstRemove.begin(); iterInst != vInstRemove.end(); ++iterInst) {      
        if (debugRTRevMemHyb)
          errs() << "removing call to: " << (dyn_cast<CallInst>(*iterInst))->getCalledFunction()->getName() << "\n";
        (*iterInst)->eraseFromParent();
      }

      return true;      
    }
    
    void print(raw_ostream &O, const Module* = 0) const { 
      errs() << "Ran Runtime Resource Estimator Validator \n";
    }  
  };
}

char RTRevMemHyb::ID = 0;
static RegisterPass<RTRevMemHyb>
X("runtime-rev-memory-manager-hybrid", "Manage reversible circuit memory at runtime");
  

