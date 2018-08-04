//===------------------- InterpretKeywords.cpp - --------------------------===//
//===--------- Interpret Scaffold program keywords that are not gates.  ---===//
//
//               The LLVM Scaffold Compiler Infrastructure
//
//        This file was created by Scaffold Compiler Working Group
//
//===----------------------------------------------------------------------===//

#include <sstream>
#include "llvm/Argument.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/ilist.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/ValueMap.h" 
#include "llvm/Constants.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Intrinsics.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"


using namespace llvm;
using namespace std;

/***********************************************************************
 * Current list of keywords
 ***********************************************************************/
/* 1. acquire(n): 
 *   - Obtain n qubits from optimized memory pool, as supposed to allocate new.
 * 2: release(out, #out, anc, #anc, cpy):
 *   - Signal to *optionally* recycle the specified qubit(s) to memory pool.
 * 3: afree(args, copy, free):
 *   - Signal to *forcefully* recycle the specified qubit(s) to memory pool.
 */

bool debugIntKey = false;

namespace {

	struct InterpretKeywords : public ModulePass {
		static char ID;
		InterpretKeywords() : ModulePass(ID) {}

    Function *CloneFunctionInfo(const Function *F, ValueMap<const Value*, WeakVH> &VMap, Module *M);

		Instruction *findAndReplace(Instruction *CI, std::map<Value*,Value*> rList, BasicBlock *BB, int whatInst);
		void buildReleaseFunction(Function *F, CallInst *CI, CallInst *ACI, Module *M, int nnOut, int nnAnc, int nnAlloc, unsigned nnGate);
		void insertNewCallSite(CallInst *CI, std::string specializedName, Module *M);

    bool runOnModule (Module &M);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesAll();  
        AU.addRequired<CallGraph>();
    }
		
	}; // struct InterpretKeywords
} // namespace


Function *InterpretKeywords::CloneFunctionInfo(const Function *F, ValueMap<const Value*, WeakVH> &VMap, Module *M) {
  std::vector<Type*> ArgTypes;
  // the user might be deleting arguments to the function by specifying them in the VMap.
  // If so, we need to not add the arguments to the ArgTypes vector

  for (Function::const_arg_iterator I = F->arg_begin(), E = F->arg_end(); I!=E; I++)
    if (VMap.count(I) == 0) // haven't mapped the argument to anything yet?
      ArgTypes.push_back(I->getType());

  // create a new funcion type...
  FunctionType *FTy = FunctionType::get(
      F->getFunctionType()->getReturnType(), ArgTypes, F->getFunctionType()->isVarArg());

  // Create the new function
  Function *NewF = Function::Create(FTy, F->getLinkage(), F->getName(), M);

  // Loop over the arguments, copying the names of the mapped arguments over...
  Function::arg_iterator DestI = NewF->arg_begin();
  for (Function::const_arg_iterator I = F->arg_begin(), E = F->arg_end(); I!=E; ++I)
    if (VMap.count(I) == 0) {     // is this argument preserved?
      DestI->setName(I->getName());   // copy the name over..
      WeakVH wval(DestI++);
      VMap[I] = wval;          // add mapping to VMap
    }
  return NewF;
}

Instruction *InterpretKeywords::findAndReplace(Instruction *CI, std::map<Value*,Value*> rList, BasicBlock *BB, int whatInst) {
	if(debugIntKey) {
		errs() << "findAndReplace: " << *CI << " at ptr: " << CI << "\n";
	}
	bool changed = false;
	if (whatInst == 0) {
		CallInst *Ins = dyn_cast<CallInst>(CI);
		Function *callFunc = Ins->getCalledFunction();
		std::vector<Value *> newArgs;
		size_t i = 0;
		for (Function::arg_iterator ai=callFunc->arg_begin(); ai != callFunc->arg_end(); ++ai) {
			Value *op = ai;//callFunc->getArgOperand(i);
			if(debugIntKey) {
				errs() << "checking : " << *op << " at ptr: " << op << "\n";
			}
			std::map<Value*, Value*>::iterator rit = rList.find(op);
			if (rit != rList.end()) {
				newArgs.push_back(rit->second);
				changed = true;
			} else {
				newArgs.push_back(op);
			}
			i += 1;
		}
		if (changed) {
			CallInst *newCall = CallInst::Create(callFunc, ArrayRef<Value*>(newArgs), "", BB);
			ReplaceInstWithInst(Ins, newCall);
			return newCall;
		} else {
			return CI;
		}
	} else if (whatInst == 1) {
		LoadInst *Ins = dyn_cast<LoadInst>(CI);
		Value *oprnd = Ins->getPointerOperand();
		Value *newOprnd;
		std::map<Value*, Value*>::iterator rit = rList.find(oprnd);
		if (rit != rList.end()) {
			newOprnd = rit->second;
			changed = true;
		}
		if (changed && newOprnd != NULL && newOprnd != oprnd) {
			LoadInst *newLoad = new LoadInst(newOprnd, "");
			newLoad->setAlignment(Ins->getAlignment());
			ReplaceInstWithInst(Ins, newLoad);
			return newLoad;
		} else {
			return CI;
		}

	} else if (whatInst == 2) {
		StoreInst *Ins = dyn_cast<StoreInst>(CI);
		Value *sval = Ins->getValueOperand();
		Value *oprnd = Ins->getPointerOperand();
		Value *newSval;
		std::map<Value*, Value*>::iterator rit = rList.find(sval);
		if (rit != rList.end()) {
			newSval = rit->second;
			// errs() << "found replacement! " << *newSval << " at " << newSval << "\n";
			changed = true;
		}
		if (changed && newSval != NULL && newSval != sval) {
			StoreInst *newStore = new StoreInst(newSval, oprnd);
			newStore->setAlignment(Ins->getAlignment());
			ReplaceInstWithInst(Ins, newStore);
			return newStore;
		} else {
			return CI;
		}
	} else if (whatInst == 3) {
		GetElementPtrInst *Ins = dyn_cast<GetElementPtrInst>(CI);
		//Value *sval = Ins->getValueOperand();
		Value *oprnd = Ins->getPointerOperand();
		Value *newOp;
		std::map<Value*, Value*>::iterator rit = rList.find(oprnd);
		if (rit != rList.end()) {
			newOp = rit->second;
			//errs() << "found replacement! " << *newOp << " at " << newOp << "\n";
			changed = true;
		}
		if (changed && newOp != NULL && newOp != oprnd) {
			//StoreInst *newStore = new StoreInst(newSval, oprnd);
			GetElementPtrInst *newGEP;
			unsigned nid = Ins->getNumIndices();
			vector<Value*>newIdxList(nid);
			size_t i = 0;
			for (GetElementPtrInst::op_iterator gop = Ins->idx_begin(); gop != Ins->idx_end(); ++gop) {
				newIdxList[i] = *gop;
				i++;
			}
			if (!Ins->isInBounds()) {
				//Type *old_ty = Ins->getPointerOperandType();
				newGEP = GetElementPtrInst::Create(newOp, ArrayRef<Value*>(newIdxList), "");
			} else {
				newGEP = GetElementPtrInst::CreateInBounds(newOp, ArrayRef<Value*>(newIdxList), "");
			}
			ReplaceInstWithInst(Ins, newGEP);
			return newGEP;
		} else {
			return CI;
		}
	} else {
		//errs() << "Instruction type not recoginized, use general Instruction class\n";
		unsigned nn = CI->getNumOperands();
		for (size_t i = 0; i < nn ; i++) {
			Value *cval = CI->getOperand(i);
			std::map<Value*, Value*>::iterator rit = rList.find(cval);
			if (rit != rList.end()) {
				Value *newCval = rit->second;
				//errs() << "found replacement! " << *newCval << " at " << newCval << "\n";
				changed = true;
				if (newCval != NULL && newCval != cval) {
					CI->setOperand(i, newCval);
				}
			}
		
		}

		return CI;
	}
}

void InterpretKeywords::buildReleaseFunction(Function *F, CallInst *CI, CallInst *ACI, Module *M, int nnOut, int nnAnc, int nnAlloc, unsigned nnGate) {
	// F is the marked caller function that contains release
	// CI is the release instruction in F
	// ACI is the acquire instruction in F
  if (debugIntKey) {
		errs() << "Building release function..." << "\n";
	}
	// Build helper function name
	std::string callerName = F->getName();

	stringstream ssf;
	ssf << "forward_" << callerName;
	std::string forwardName = ssf.str();
	Function *forwardImpl = M->getFunction(forwardName);

	stringstream ssr;
	ssr << "_reverse_forward_" << callerName;
	std::string revName= ssr.str();
	Function *revImpl= M->getFunction(revName);

	// Build function arguments
	size_t Fnumargs = F->arg_size();
	// 0~Fnumargs-1: caller arguments; 
	// +0: out; +1: #out; +2: anc; +3: #anc, +4: cpy; +5: inter; +6: #inter
	std::vector<Type*> ArgTypes(Fnumargs+4+1+2);
	std::vector<Value*>  Args(Fnumargs+4+1+2);
	std::vector<Value*>  newArgs(Fnumargs+4+1+2);
	std::vector<Value*>  newForwardArgs(Fnumargs+4+1+2);
	std::vector<Value*>  newRevArgs(Fnumargs+4+1+2);
	// First original arguments from F
	size_t i = 0;
	for (Function::arg_iterator ai=F->arg_begin(); ai != F->arg_end(); ++ai) {
		ArgTypes[i] = ai->getType();
		//Args[i] = F->getArgOperand(i);
		Args[i] = ai;
		i++;
	}
	//errs() << "here1: " << *ACI << "\n";
	// Then acquire
	if (ACI->getNumArgOperands() != 4) {
		errs() << "Error: Acquire needs 4 arguments, " << ACI->getNumArgOperands()<< " found.\n"; 
	}
	if (CI->getNumArgOperands() != 5) {
		errs() << "Error: Release needs 5 arguments, " << CI->getNumArgOperands()<< " found.\n"; 
	}
	//errs() << "here1.5\n";
	//errs() << "ACI 0: " << *(ACI->getArgOperand(0)) << " ACI 1: " << *(ACI->getArgOperand(1)) << "\n";
	//ArgTypes[Fnumargs+4] = ACI->getCalledFunction()->getReturnType();
	//if (GetElementPtrInst *Anc = dyn_cast<GetElementPtrInst>(ACI->getArgOperand(1))) {
	//	ArgTypes[Fnumargs+4] = Anc->getPointerOperand()->getType();
	//	Args[Fnumargs+4] = Anc->getPointerOperand();
	//	ArgTypes[Fnumargs+2] = Anc->getPointerOperand()->getType();
	//	Args[Fnumargs+2] = Anc->getPointerOperand();
	//	//ArgTypes[Fnumargs+3] = ACI->getArgOperand(0)->getType();
	//} else {
		ArgTypes[Fnumargs+5] = ACI->getArgOperand(3)->getType();
		Args[Fnumargs+5] = ACI->getArgOperand(3); // interaction bits
		ArgTypes[Fnumargs+2] = CI->getArgOperand(2)->getType(); // anc
		Args[Fnumargs+2] = CI->getArgOperand(2); // anc
		ArgTypes[Fnumargs+4] = CI->getArgOperand(4)->getType(); // cpy
		Args[Fnumargs+4] = CI->getArgOperand(4); // cpy
		//ArgTypes[Fnumargs+3] = CI->getArgOperand(3)->getType(); // free
	//}
	ArgTypes[Fnumargs+3] = CI->getArgOperand(3)->getType(); // #anc
	ArgTypes[Fnumargs+6] = ACI->getArgOperand(2)->getType(); // #inter
	size_t nAlloc;
	ConstantInt *aciAlloc;
	//errs() << "here1.75\n";
	if (nnAlloc != -1) {
		nAlloc = nnAlloc;
		aciAlloc = dyn_cast<ConstantInt>(ConstantInt::get(ArgTypes[Fnumargs+1], nAlloc, false)); 
		//Args[Fnumargs+6] = aciAlloc;
		Args[Fnumargs+6] = ACI->getArgOperand(2);
	} else {
		aciAlloc = dyn_cast<ConstantInt>(ACI->getArgOperand(0));
		if (aciAlloc == NULL) {
			errs() << "Error: Does not support variable length allocation yet! Try loop-unrolling and constant propagation.\n";
		} else {
			nAlloc = aciAlloc->getZExtValue(); // number of out bits allocated
			//Args[Fnumargs+6] = aciAlloc;
			Args[Fnumargs+6] = ACI->getArgOperand(2);
		}
	}
	//errs() << "here2\n";
	// Now release
	if (CI->getNumArgOperands() != 5) {
		errs() << "Error: Release needs 5 arguments, " << CI->getNumArgOperands()<< " found.\n"; 
	}
	ArgTypes[Fnumargs+0] = CI->getArgOperand(0)->getType(); // out
	ArgTypes[Fnumargs+1] = CI->getArgOperand(1)->getType(); // out
	Args[Fnumargs+0] = CI->getArgOperand(0); // out
	size_t nOut;
	ConstantInt *ciOut;
	if (nnOut != -1) {
		nOut = nnOut;
		ciOut = dyn_cast<ConstantInt>(ConstantInt::get(ArgTypes[Fnumargs+1], nOut, false)); 
		Args[Fnumargs+1] = ciOut;
	} else {
		ciOut = dyn_cast<ConstantInt>(CI->getArgOperand(1));
		nOut = ciOut->getZExtValue(); // number of out bits to copy
		Args[Fnumargs+1] = ciOut;
	}

	//ArgTypes[Fnumargs+2] = CI->getArgOperand(2)->getType(); // free
	//ArgTypes[Fnumargs+3] = CI->getArgOperand(3)->getType(); // free
	//Args[Fnumargs+2] = CI->getArgOperand(2); // free

	size_t nAnc;
	ConstantInt *ciAnc;
	if (nnAnc != -1) {
		nAnc = nnAnc;
		ciAnc = dyn_cast<ConstantInt>(ConstantInt::get(ArgTypes[Fnumargs+3], nAnc, false));
		Args[Fnumargs+3] = ciAnc;
	} else {
		ciAnc = dyn_cast<ConstantInt>(CI->getArgOperand(3));
		nAnc = ciAnc->getZExtValue(); // number of anc bits to free
		Args[Fnumargs+3] = ciAnc;
	}

	// Build function name
  std::stringstream ss; 
	ss << "_Release_O_" << nOut << "_A_" << nAnc << "_G_" << nnGate << callerName;
	std::string releaseName = ss.str(); //CI->getCalledFunction()->getName();


	// Check if already implemented
	Function *releaseImpl = M->getFunction(releaseName);
  if (debugIntKey) {
		errs() << "check if impemented\n";
	}
	// Now implement the new release function
	unsigned gateCount = nOut; // nOut CNOT gates for copying
	if (!releaseImpl) {
		
		//FunctionType *FuncType = FunctionType::get(Type::getVoidTy(getGlobalContext()),
		FunctionType *FuncType = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
	         	     ArrayRef<Type*>(ArgTypes), false);
		releaseImpl = Function::Create(FuncType, GlobalVariable::ExternalLinkage, releaseName, M);
	
		Function::arg_iterator arg_it = releaseImpl->arg_begin();
		for (size_t i = 0; i < Args.size(); i++) {
			Value *new_arg = arg_it++;
  	  std::stringstream ss;
			ss << "q" << i;
			new_arg->setName(ss.str());
			newArgs[i] = new_arg;
		}


		//errs() << "check1\n";
		// Create basic block
		BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "", releaseImpl, 0);
		std::map<Value*, Value*> ArgsMap; // copied qubits caller to release qubits

		// Local variables for function paramters
		std::vector<Value*> newArgAddr; 
		for (Function::arg_iterator AI = releaseImpl->arg_begin(), AE = releaseImpl->arg_end(); AI != AE; ++AI)
		{
		    Value* v = &*AI;
				stringstream ss;
				ss << v->getName().str() << ".addr";
				AllocaInst *local = new AllocaInst(v->getType(), ss.str(), BB);
				TargetData TD = TargetData(M);
				local->setAlignment(TD.getTypeAllocSize(v->getType()));

				StoreInst *local2 = new StoreInst(v, (Value*)local, false, BB);
				local2->setAlignment(TD.getTypeAllocSize(v->getType()));
				Value *inArg = new LoadInst(local, "", BB);
				newArgAddr.push_back(inArg);
				
		}

		errs() << "check2\n";
		// If cpy bits are NULL, then:
		// Initialize new qubits with alloca and acquire 
		// else: use cpy

		vector<Value*> cpy_bits;
		// create predicate
		//Value *in_cpy = new LoadInst(newArgAddr[Fnumargs+4], "", BB);
		//Value *pred = new ICmpInst(*BB, CmpInst::ICMP_EQ, in_cpy, Constant::getNullValue(Type::getInt16Ty(getGlobalContext())->getPointerTo()), "");
		Value *pred = new ICmpInst(*BB, CmpInst::ICMP_EQ, newArgAddr[Fnumargs+4], Constant::getNullValue(Type::getInt16Ty(getGlobalContext())->getPointerTo()->getPointerTo()), "");


		std::string new_name = "new";
		Type *qbit_type = Type::getInt16Ty(getGlobalContext())->getPointerTo();// ArgTypes[Fnumargs]; 
		ArrayType *arrayType = ArrayType::get(qbit_type, nOut);
		AllocaInst *new_copy = new AllocaInst(arrayType, new_name, BB);
		TargetData TD = TargetData(M);
		new_copy->setAlignment(TD.getTypeAllocSize(qbit_type));
		Value *Idx[2];
		Idx[0] = Constant::getNullValue(Type::getInt32Ty(getGlobalContext()));  
		Idx[1] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),0);
	
		Value *qqArrPtr = GetElementPtrInst::CreateInBounds(new_copy, Idx, "", BB);

		// if true
		BasicBlock *nullBB = BasicBlock::Create(getGlobalContext(), "", releaseImpl, 0);
		Function *new_acquire = M->getFunction("acquire");
		if (new_acquire == NULL) {
			errs() << "Does not recognize acquire!\n";
		}
		std::vector<Type*> na_ty;
		na_ty.push_back(Type::getInt32Ty(getGlobalContext()));
		na_ty.push_back(qqArrPtr->getType());
		na_ty.push_back(Type::getInt32Ty(getGlobalContext())); // interaction
		na_ty.push_back(Type::getInt16Ty(getGlobalContext())->getPointerTo()->getPointerTo()); // interaction bits
		//na_ty.push_back(Type::getInt16Ty(getGlobalContext())->getPointerTo()->getPointerTo()); // interaction bits
		std::vector<Value*> na_val;
		na_val.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()),nOut));
		na_val.push_back(qqArrPtr);
		//na_val.push_back(ACI->getArgOperand(2));
		//na_val.push_back(ACI->getArgOperand(3));
		na_val.push_back(newArgAddr[Fnumargs+6]); // #interaction
		na_val.push_back(newArgAddr[Fnumargs+5]); // interaction

		errs() << "check3\n";
		CallInst::Create(new_acquire, ArrayRef<Value*>(na_val), "", nullBB)->setTailCall();
		errs() << "check4\n";


		// if false

		BasicBlock *nonnullBB = BasicBlock::Create(getGlobalContext(), "", releaseImpl, 0);
		for (size_t i = 0; i < nOut; i++) {
			//Value *Idx[2];
			//Idx[0] = Constant::getNullValue(Type::getInt32Ty(getGlobalContext()));  
			//Idx[1] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),i);
			//Value *intArrPtr = GetElementPtrInst::CreateInBounds(new_copy, Idx, "", BB);
			//Value *new_q = new LoadInst(intArrPtr, "", BB);
			//cpy_bits.push_back(new_q);	

			stringstream ss;
			ss << "arrayIdxC" << i;
			Value *newIdx[1];
			newIdx[0] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),i);
			Value *intArrPtr2 = GetElementPtrInst::CreateInBounds(newArgAddr[Fnumargs+4], newIdx, ss.str(), nonnullBB);
			Value *old_q = new LoadInst(intArrPtr2, "", nonnullBB);
			Value *newIdx2[2];
			newIdx2[0] = Constant::getNullValue(Type::getInt32Ty(getGlobalContext()));  
			newIdx2[1] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),i);
			Value *targArrPtr = GetElementPtrInst::CreateInBounds(new_copy, newIdx2, "", nonnullBB);
			
			new StoreInst(old_q, targArrPtr, false, nonnullBB);
			//cpy_bits.push_back(old_q);	
		}

		//errs() << "check end\n";
		// end if block
		BasicBlock *endBB = BasicBlock::Create(getGlobalContext(), "", releaseImpl, 0);
		// both if and else br to endBB
		BranchInst *forIf = BranchInst::Create(endBB, nullBB);
		BranchInst *forElse = BranchInst::Create(endBB, nonnullBB);
		//
		// insert the conditional branch into BB
		TerminatorInst *oldTerm = BB->getTerminator();
		BranchInst *brTerm;
		//errs() << "check end end" << oldTerm << "\n";
		if (oldTerm == NULL) {
			brTerm = BranchInst::Create(nullBB, nonnullBB, pred, BB);
		} else {
			brTerm = BranchInst::Create(nullBB, nonnullBB, pred);
			ReplaceInstWithInst(oldTerm, brTerm);
		}
		//errs() << "check end end\n";

		for (size_t i = 0; i < nOut; i++) {
			Value *Idx[2];
			Idx[0] = Constant::getNullValue(Type::getInt32Ty(getGlobalContext()));  
			Idx[1] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),i);
			Value *intArrPtr = GetElementPtrInst::CreateInBounds(new_copy, Idx, "", endBB);
			Value *new_q = new LoadInst(intArrPtr, "", endBB);
			cpy_bits.push_back(new_q);	
		}
		// Setup qubits for Copying
		vector<Value*> out_bits;
		for (size_t i = 0; i < nOut; i++) {
			stringstream ss;
			ss << "arrayIdx" << i;
			Value *newIdx[1];
			newIdx[0] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),i);
			Value *intArrPtr2 = GetElementPtrInst::CreateInBounds(newArgAddr[Fnumargs], newIdx, ss.str(), endBB);
			Value *old_q = new LoadInst(intArrPtr2, "", endBB);
			out_bits.push_back(old_q);	
		}
		//errs() << "check5\n";


		// Copy output
		for (size_t i = 0; i < nOut; i++) {
			//Value *out_bit = new LoadInst(out_bits[i], "", BB);
			//Value *cpy_bit = new LoadInst(cpy_bits[i], "", BB);
			Value *out_bit = out_bits[i];
			Value *cpy_bit = cpy_bits[i];
			std::vector<Type*> two_qbit_ty;
			two_qbit_ty.push_back(out_bit->getType());
			two_qbit_ty.push_back(cpy_bit->getType());
			std::vector<Value*> src_trg;
			src_trg.push_back(out_bit);
			src_trg.push_back(cpy_bit);
			Function *cnot_copy = Intrinsic::getDeclaration(M, Intrinsic::CNOT, ArrayRef<Type*>(two_qbit_ty));
			CallInst::Create(cnot_copy, ArrayRef<Value*>(src_trg), "", endBB)->setTailCall();
			
		}
		//errs() << "check6\n";
		// Perform dummy operation on anc, so that llvm doesn't optimize them away
		//vector<Value*> anc_bits;
		//ConstantInt *zero = ConstantInt::get(Type::getInt16Ty(getGlobalContext()),0);
		//for (size_t i = 0; i < nAnc; i++) {
		//	stringstream ss;
		//	ss << "arrayIdxA" << i;
		//	Value *newIdx[1];
		//	newIdx[0] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),i);
		//	Value *intArrPtr2 = GetElementPtrInst::CreateInBounds(newArgAddr[Fnumargs+2], newIdx, ss.str(), BB);
		//	Value *anc_bit_ptr = new LoadInst(intArrPtr2, "", BB);
		//	// Value *anc_bit = new LoadInst(anc_bit_ptr, "", BB);
		//	StoreInst *stored = new StoreInst(zero, anc_bit_ptr, false, BB);
		//	TargetData TD = TargetData(M);
		//	stored->setAlignment(TD.getTypeAllocSize(zero->getType()));
		//}
		//errs() << "check7\n";

		// Reverse caller
		std::map<Value*, Value*> var_map;
		for (size_t i = 0; i < Args.size(); i++) {
			var_map[Args[i]] = newArgs[i];
		}	

		//Build the forward_<F> function, and then call _reverse_forward_<F>
		// Check if already implemented
		if (!forwardImpl) {
			FunctionType *FuncType = FunctionType::get(Type::getVoidTy(getGlobalContext()),
		         	     ArrayRef<Type*>(ArgTypes), false);
			forwardImpl = Function::Create(FuncType, GlobalVariable::ExternalLinkage, forwardName, M);
		
			Function::arg_iterator arg_it = forwardImpl->arg_begin();
			for (size_t i = 0; i < Args.size(); i++) {
				Value *new_arg = arg_it++;
  		  std::stringstream ss;
				ss << "q" << i;
				new_arg->setName(ss.str());
				newForwardArgs[i] = new_arg;
			}

			// Create basic block
			//BasicBlock *BBforward = BasicBlock::Create(getGlobalContext(), "", forwardImpl, 0);
      ValueMap<const Value*, WeakVH> VMap;
			for (size_t i = 0; i < Args.size(); i++) {
				if (i == Fnumargs+1 || i == Fnumargs+3 || i == Fnumargs+4 || i == Fnumargs+6) {
					continue;
				}
				WeakVH wval(newForwardArgs[i]);
				VMap[Args[i]] = wval;
  			if (debugIntKey) {
					errs() << "old(" << *(ArgTypes[i]) << "): " << *(Args[i]) << " new("<< "): " << *(newForwardArgs[i]) << " weakVH: " << *wval << ", i.e. " << *(VMap[Args[i]]) << "\n";
				}
			}
    	SmallVector<ReturnInst*,1> Returns; // FIXME: what is the length of this vector?
    	//ClonedCodeInfo newCodeInfo;

			CloneFunctionInto(forwardImpl,   				// newFunc
                        F,         						// OldFunc
                        VMap,                 // ValueMap
                        0,                    // ModuleLevelChanges
                        Returns,              // Returns
                        "",                  // NameSuffix
                        0,                    // CodeInfo
                        0);                   // TD            

			// Since forwardImpl has 6 more arguments than F
			// so, still need to take care of two things:
			//  1. replace all ancilla declared with input parameter
			//  2. remove the release call itself.
      std::vector<CallInst *> V;
      std::map<Value *, Value *> toReplace;
			for (Function::iterator b = forwardImpl->begin(), be = forwardImpl->end(); b != be; ++b) {
				//BasicBlock &BBforward = forwardImpl->front();
				BasicBlock *BBforward = b;
				BasicBlock::iterator bbit;
      	BasicBlock::iterator E = BBforward->end();
      	CallInst *CI;
      	for (bbit = BBforward->begin(); bbit != E; bbit++) {
      	  if ( (CI = dyn_cast<CallInst>(&*bbit)) ) {
      	  	V.push_back(CI);
						// Now prepare the parameters to be replaced
						Function *rf = CI->getCalledFunction();
						if (!rf->isIntrinsic() ) {
							// TODO: extract operands: (args, copy, free)
      				std::string rfname = rf->getName().str(); 
      			  std::string::size_type rfCoreEnd;
      			  rfCoreEnd = rfname.find(std::string("_IP"));
      			  std::string rfCore = rfname.substr(0, rfCoreEnd);
							if (rfCore == "release") {
								// Need to take care of the new paramters
								size_t nao = CI->getNumArgOperands();
								if (nao != 4) {
									errs() << "Error: release have unmatched number of arguments after cloning.\n";
								}
								for (size_t i = 0; i < nao; i++) {
									Value *op = CI->getArgOperand(i);
									Value *newOp = newForwardArgs[Fnumargs+i];
									if (op != newOp) {
										//op->replaceAllUsesWith(newOp);
										//errs() << "op: " << *op << " at " << op << "\n";
										//errs() << "newOp: " << *newOp << " at " << newOp << "\n";
										toReplace[op] = newOp;
									}
								}
							} else if (rfCore == "acquire") {
								// Need to take care of the new paramters
								Value *op = CI;
								Value *newOp = newForwardArgs[Fnumargs+4];
								if (op != newOp) {
									//op->replaceAllUsesWith(newOp);
									//errs() << "op: " << *op << " at " << op << "\n";
									//errs() << "newOp: " << *newOp << " at " << newOp << "\n";

									toReplace[op] = newOp;
								}
								//errs()<< "checkpoint\n";
								// replace acquire with store inst from input parameters
								ConstantInt *nAv = dyn_cast<ConstantInt>(CI->getArgOperand(0)); // num of bits to acquire
								GetElementPtrInst *localA = dyn_cast<GetElementPtrInst>(CI->getArgOperand(1));
								if (nAv != NULL && localA != NULL) {
									size_t nA = nAv->getZExtValue();
									Value *ptrA = localA->getPointerOperand();
									for (size_t i = 0; i < nA; i++) {
										stringstream ss;
										ss << "arrayIdxAA" << i;
										Value *newIdx[1];
										newIdx[0] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),i);
										Value *intArrPtr2 = GetElementPtrInst::CreateInBounds(newForwardArgs[Fnumargs+2], newIdx, ss.str(), CI);
										Value *anc_bit_ptr = new LoadInst(intArrPtr2, "", CI);
										// Value *anc_bit = new LoadInst(anc_bit_ptr, "", BB);
										stringstream ss2;
										ss2 << "arrayDecayA" << i;
										Value *decayIdx[2];
										decayIdx[0] = Constant::getNullValue(Type::getInt32Ty(getGlobalContext()));  
										decayIdx[1] = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),i);
										Value *decayPtr2 = GetElementPtrInst::CreateInBounds(ptrA, decayIdx, ss2.str(), CI);
										StoreInst *stored = new StoreInst(anc_bit_ptr, decayPtr2, false, CI);
										TargetData TD = TargetData(M);
										stored->setAlignment(TD.getTypeAllocSize(anc_bit_ptr->getType()));
									}
								}

							}
						}	else {
							// is intrinsic
							unsigned iid = rf->getIntrinsicID();
							if (iid == Intrinsic::CNOT || iid == Intrinsic::Toffoli ||
							    iid == Intrinsic::Fredkin  || iid == Intrinsic::H ||
							    iid == Intrinsic::X  || iid == Intrinsic::Y ||
							    iid == Intrinsic::Z  || iid == Intrinsic::Rx ||
							    iid == Intrinsic::Ry  || iid == Intrinsic::Rz ||
							    iid == Intrinsic::S  || iid == Intrinsic::Sdag ||
							    iid == Intrinsic::T  || iid == Intrinsic::Tdag ||
							    iid == Intrinsic::PrepX  || iid == Intrinsic::PrepZ ||
							    iid == Intrinsic::MeasX  || iid == Intrinsic::MeasZ) {
								gateCount += 1;
							}
						}
					}
				}
			}
			if (debugIntKey) {
				errs() << "size: " << V.size() << " gateCount: " << gateCount << "\n";
				if (gateCount != nnGate) {
					errs() << "Warning: gateCount: " << gateCount << " not equals nnGate: " << nnGate << "\n";
				}
			}
			for (Function::iterator b = forwardImpl->begin(), be = forwardImpl->end(); b != be; ++b) {
				BasicBlock *BBforward = b;
				BasicBlock::iterator bbit;
				Instruction *newIns;
      	CallInst *CI;
      	CallInst *newCI;
      	LoadInst *LI;
      	LoadInst *newLI;
      	StoreInst *SI;
      	StoreInst *newSI;
      	GetElementPtrInst *GI;
      	GetElementPtrInst *newGI;
      	for (bbit = BBforward->begin(); bbit != BBforward->end(); ++bbit) {
      	  if ( (CI = dyn_cast<CallInst>(&*bbit)) ) {
						newIns = findAndReplace(CI, toReplace, BBforward, 0);
						newCI = dyn_cast<CallInst>(newIns);
						if (newCI != CI) {
							bbit = BBforward->begin();
						}
					} else if ( (LI = dyn_cast<LoadInst>(&*bbit)) ) {
						newIns = findAndReplace(LI, toReplace, BBforward, 1);
						newLI = dyn_cast<LoadInst>(newIns);
						if (newLI != LI) {
							bbit = BBforward->begin();
						}
					} else if ( (SI = dyn_cast<StoreInst>(&*bbit)) ) {
						newIns = findAndReplace(SI, toReplace, BBforward, 2);
						newSI = dyn_cast<StoreInst>(newIns);
						if (newSI != SI) {
							bbit = BBforward->begin();
						}
					} else if ( (GI = dyn_cast<GetElementPtrInst>(&*bbit)) ) {
						newIns = findAndReplace(GI, toReplace, BBforward, 3);
						newGI = dyn_cast<GetElementPtrInst>(newIns);
						if (newGI != GI) {
							bbit = BBforward->begin();
						}
					} else {
						Instruction *oldIns = dyn_cast<Instruction>(&*bbit);
						newIns = findAndReplace(oldIns, toReplace, BBforward, 4);
						if (newIns != oldIns) {
							bbit = BBforward->begin();
						}
					}
				}
			}
			for (Function::iterator b = forwardImpl->begin(), be = forwardImpl->end(); b != be; ++b) {
				BasicBlock *BBforward = b;
				BasicBlock::iterator bbit;
      	CallInst *newCI;
	
				if (debugIntKey) {
					errs() << "Erasing copied release call\n";
				}
      	for (bbit = BBforward->begin(); bbit != BBforward->end(); ++bbit) {
      	  if ( (newCI = dyn_cast<CallInst>(&*bbit)) ) {
						Function *rf = newCI->getCalledFunction();
						if (!rf->isIntrinsic() ) {
							// TODO: extract operands: (args, copy, free)
      				std::string rfname = rf->getName().str(); 
      			  std::string::size_type rfCoreEnd;
      			  rfCoreEnd = rfname.find(std::string("_IP"));
      			  std::string rfCore = rfname.substr(0, rfCoreEnd);
							if (rfCore == "release") {
      					newCI->eraseFromParent();
								bbit = BBforward->begin();
      					//E = BBforward.end();
							}
						}
					}
				}
			}
			for (Function::iterator b = forwardImpl->begin(), be = forwardImpl->end(); b != be; ++b) {
				BasicBlock *BBforward = b;
				BasicBlock::iterator bbit;
      	CallInst *newCI;
	
				if (debugIntKey) {
					errs() << "Erasing copied acquire call\n";
				}
      	for (bbit = BBforward->begin(); bbit != BBforward->end(); ++bbit) {
      	  if ( (newCI = dyn_cast<CallInst>(&*bbit)) ) {
						Function *rf = newCI->getCalledFunction();
						if (!rf->isIntrinsic() ) {
							// TODO: extract operands: (args, copy, free)
      				std::string rfname = rf->getName().str(); 
      			  std::string::size_type rfCoreEnd;
      			  rfCoreEnd = rfname.find(std::string("_IP"));
      			  std::string rfCore = rfname.substr(0, rfCoreEnd);
	
							if (rfCore == "acquire") {
								//if (debugIntKey) {
								//	errs() << "erasing acquire\n";
								//	errs() << "VMap[0]: " << *(Args[0]) << " -> " << *(VMap[Args[0]]) << "\n";
								//	errs() << "VMap[Fnumargs+2]: " << (Args[Fnumargs+2]) << " and " << newCI << "\n";
								//	errs() << "VMap[Fnumargs+2]: " << *(Args[Fnumargs+2]) << " and " << *newCI << "\n";
								//	errs() << "VMap[Fnumargs+2]ptr: " << Args[Fnumargs+2] << " -> " << VMap[Args[Fnumargs+2]] << "\n";
								//	errs() << "VMap[Fnumargs+2]: " << *(Args[Fnumargs+2]) << " -> " << *(VMap[Args[Fnumargs+2]]) << "\n";
								//	errs() << "mapped? " << (VMap[newCI] != NULL) << " and uses: " << newCI->getNumUses() << "\n";
								//}

								// Replace with array indexing??
      					newCI->eraseFromParent();
								bbit = BBforward->begin();
							}
      			}
      		}
					//ReturnInst::Create(getGlobalContext(), 0, BBforward);
				}
			}	
		}
		//ReplaceInstWithInst(CI, CallInst::Create(releaseImpl, ArrayRef<Value*>(Args)));
	
		// Write down delaration of _reverse_forward<F>
		// Reverse Pass will occupy this function later

		if (!revImpl) {
			FunctionType *FuncType = FunctionType::get(Type::getVoidTy(getGlobalContext()),
		         	     ArrayRef<Type*>(ArgTypes), false);
			revImpl = Function::Create(FuncType, GlobalVariable::ExternalLinkage, revName, M);
		
			Function::arg_iterator arg_it = revImpl->arg_begin();
			for (size_t i = 0; i < Args.size(); i++) {
				Value *new_arg = arg_it++;
  		  std::stringstream ss;
				ss << "q" << i;
				new_arg->setName(ss.str());
				newRevArgs[i] = new_arg;
			}

		}

		// Now back to releaseImpl
		// First insert the _reverse_ function.
		CallInst::Create(revImpl, ArrayRef<Value*>(newArgs), "", endBB)->setTailCall();

		ConstantInt *gateCountV = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),gateCount);
		ReturnInst::Create(getGlobalContext(), gateCountV, endBB);

	} //end releaseImpl
	// Replace call instruction
	if (debugIntKey) {
		errs() << "replacing instruction\n";
	}

	
	CallInst::Create(releaseImpl, ArrayRef<Value*>(Args), "", CI);
	CI->eraseFromParent();
	// ReplaceInstWithInst(CI, CallInst::Create(releaseImpl, ArrayRef<Value*>(Args)));
	if (debugIntKey) {
		errs() << "done replacing instruction\n";
	}
	return;
}

void InterpretKeywords::insertNewCallSite(CallInst *CI, std::string specializedName, Module *M) {
  CallSite CS = CallSite(CI);
  std::vector<Value*> Args;
  Args.reserve(CS.arg_size());
  CallSite::arg_iterator AI = CS.arg_begin();
	unsigned int e = CI->getCalledFunction()->getFunctionType()->getNumParams();
  for (unsigned i = 0; i!=e; ++i, ++AI) {
		//copy arguments FIXME: delete int args
    Args.push_back(*AI); 
	}
  ArrayRef<Value*> ArgsRef(Args);
  
  CallInst* newCall = CallInst::Create(M->getFunction(specializedName), ArgsRef, "", (Instruction*)CI);
  newCall -> setCallingConv (CS.getCallingConv());
  if (CI -> isTailCall())
    newCall -> setTailCall();
}



bool InterpretKeywords::runOnModule (Module &M) {
  // iterate over all functions, and over all instructions in those functions
  // find call sites that have constant integer or double values.
  if (debugIntKey) {
    errs() << "Interpreting keywords.."  << "\n";
	}

  CallGraphNode* rootNode = getAnalysis<CallGraph>().getRoot();
  
  std::vector<Function*> vectPostOrder;
  
  for (scc_iterator<CallGraphNode*> sccIb = scc_begin(rootNode), E = scc_end(rootNode); sccIb != E; ++sccIb) {
    const std::vector<CallGraphNode*> &nextSCC = *sccIb;
    for (std::vector<CallGraphNode*>::const_iterator nsccI = nextSCC.begin(), E = nextSCC.end(); nsccI != E; ++nsccI) {
      Function *f = (*nsccI)->getFunction();	  
      
      if(f && !f->isDeclaration())
        vectPostOrder.push_back(f);
    }
  }

  //unsigned int initial_vector_size = (unsigned int)(vectPostOrder.size());
  
  //reverse the vector
  std::reverse(vectPostOrder.begin(),vectPostOrder.end());

  //--- start traversing in reverse order for a pre-order

  // keep track of which functions are cloned
  // and need not be processed when we reach them
  std::vector <Function*> funcErase;
  for(std::vector<Function*>::iterator vit = vectPostOrder.begin(); vit!=vectPostOrder.end(); ++vit) { 
    Function *f = *vit;      
    std::string fname = f->getName();

    if (debugIntKey) {
      errs() << "---------------------------------------------" << "\n";
      errs() << "Caller: " << fname << "\n";
    }

    std::string::size_type fCoreEnd;
    fCoreEnd = fname.find(std::string("_IP"));
	  if (fCoreEnd == std::string::npos) // maybe it's all doubles?
	  	fCoreEnd = fname.find(std::string("_DP"));        
    std::string fCore = fname.substr(0, fCoreEnd);
    // what instructions (call sites) need to be erased after this function has been processed
    std::vector <Instruction*> instErase;
    // in vectPostOrder traversal, when reaching a function that has been marked for deletion...
    // skip and do not inspect it anymore
    //if (std::find(funcErase.begin(), funcErase.end(), (*vit)) != funcErase.end()) {
    //  if (debugIntKey)
    //    errs() << "Skipping...: " << (*vit)->getName() << "\n";  
    //  continue;
    //}      
    bool markedA = false;
    bool markedR = false;
    bool classical = true;
		CallInst *RCI = NULL; // release
		CallInst *ACI = NULL; // acquire
		int nOut = -1;
		int nAnc = -1;
		int nAlloc = -1;
		int nGate = -1;
		int counter = 0;
    for (inst_iterator I = inst_begin(*f); I != inst_end(*f); ++I) {
			// Check if function is classical and contain release/afree
      Instruction *pInst = &*I;
      if(CallInst *CI = dyn_cast<CallInst>(pInst)) {
        Function *F = CI->getCalledFunction();
				if (!F->isIntrinsic() ) {
        	std::string iname = F->getName().str(); 
          std::string::size_type iCoreEnd;
          iCoreEnd = iname.find(std::string("_IP"));
          std::string iCore = iname.substr(0, iCoreEnd);
					// assume that if exists acuqire/release, each appear exactly once in a function
					if (iCore == "acquire") {
						// extract ptr and number
						if (debugIntKey) {
							errs() << "New ancilla requested by caller: " << fname << "\n";
						}
						markedA = true;
						ACI = CI;
						nAlloc = -1;
					} else if (iCore == "release") {
						if (debugIntKey) {
							errs() << "Uncomputation requested by caller: " << fname << "\n";
						}
						// extract operands: (args, copy, free)
						markedR = true;
						RCI = CI;
						nOut = -1;
						nAnc = -1;
						nGate = counter;
						if (iCoreEnd != std::string::npos) {
            	std::string::size_type found_pos_begin, found_pos_end, found_pos_begin_new;
            	std::vector<int> originalInts; 
					  	found_pos_begin = iCoreEnd;
            	while (found_pos_begin != std::string::npos){
            	  //because there might be numbers more than 1 digit long, need to find begin and end
            	  found_pos_end = iname.find_first_not_of("012345679-x",found_pos_begin+3);
            	  std::string intString = iname.substr(found_pos_begin+3, found_pos_end-(found_pos_begin+3));
            	  if(intString!=std::string("x"))
            	    originalInts.push_back(atoi(intString.c_str()));
            	  //next one
            	  found_pos_begin_new = iname.find("_IP",found_pos_end);  
            	  found_pos_begin = found_pos_begin_new;
            	}
							if (originalInts.size() >= 2) {
								nOut = originalInts[0];
								nAnc = originalInts[1];
							} else {
								errs() << "Error: Cannot read parameters from " << iname << "\n";
							}

						}
					}
				} else {
					// check intrinsic gates (TODO: recursively?)
					// is intrinsic
					unsigned iid = F->getIntrinsicID();
					if (iid == Intrinsic::CNOT || iid == Intrinsic::Toffoli ||
					    iid == Intrinsic::Fredkin  || iid == Intrinsic::H ||
					    iid == Intrinsic::X  || iid == Intrinsic::Y ||
					    iid == Intrinsic::Z  || iid == Intrinsic::Rx ||
					    iid == Intrinsic::Ry  || iid == Intrinsic::Rz ||
					    iid == Intrinsic::S  || iid == Intrinsic::Sdag ||
					    iid == Intrinsic::T  || iid == Intrinsic::Tdag ||
					    iid == Intrinsic::PrepX  || iid == Intrinsic::PrepZ ||
					    iid == Intrinsic::MeasX  || iid == Intrinsic::MeasZ) {
						counter += 1;
					}
					if (iid != Intrinsic::X &&
					    iid != Intrinsic::CNOT &&
					    iid != Intrinsic::Toffoli &&
					    iid != Intrinsic::Fredkin) {
						// not a classical reversible function
						classical = false;
					}
				}
			}
		}

		if (!markedR || !RCI || !markedA || !ACI) {
    	if (debugIntKey) {
      	errs() << "Complete: " << fname << "\n\n";
			}
			continue;
		} else if (!classical) {
			errs() << "Error: Cannot uncompute a non-classical function: " << fname << "\n";
		}

		// Start implementing release/afree function
		buildReleaseFunction(f, RCI, ACI, &M, nOut, nAnc, nAlloc, nGate);
		if (debugIntKey) {
     	errs() << "Complete: " << fname << "\n\n";
		}
	} 

	return true;

} // End runOnModule


char InterpretKeywords::ID = 0;
static RegisterPass<InterpretKeywords> X("InterpretKeywords", "Interpret Scaffold keywords", false, false);

