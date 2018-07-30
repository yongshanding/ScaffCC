//===- CreateGates.cpp - Transform custom gates  -------------------===//
//===------------- into calls to intrinsic function that cannot ---------------===//
//===--------------  be eliminated by deadcode_elim  ----------------------===//
//
//                     The LLVM Scaffold Compiler Infrastructure
//
//        This file was created by Scaffold Compiler Working Group
//
//===----------------------------------------------------------------------===//

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
#include "llvm/Constants.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Intrinsics.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"


using namespace llvm;
using namespace std;

namespace {
	// We need to use a ModulePass in order to create new Functions
	struct CreateGates : public ModulePass {
		static char ID;
		CreateGates() : ModulePass(ID) {}

		struct CGVisitor : public InstVisitor<CGVisitor> {
			Module *M;
			// The constructor is called once per module (in runOnModule)
			CGVisitor(Module *module) : M(module) {}

			void visitCallInst(CallInst &CI) {
				// Determine whether this is a new gate function 
				Function *Func = CI.getCalledFunction();
				std::string funcName1 = Func->getName();
				if(Func->getName().find("gate_") != string::npos || 
					Func->getName().find("gate::") != string::npos){
	  				unsigned numOps = CI.getNumOperands();
					std::string funcNameOrig = Func->getName();
	  				std::string funcName = funcNameOrig.substr(5);
					vector<Type*> funcTypes;
					vector<Value*> funcValues;
  					for(unsigned int iop = 0; iop < numOps-1; iop++){
						Value* op = CI.getArgOperand(iop);
						Type* opType = op->getType();
						if (opType->isIntegerTy(16)){
							funcTypes.push_back(Type::getInt16Ty(getGlobalContext()));
							funcValues.push_back(op);
						}
						else if (opType->isIntegerTy(32)){
							funcTypes.push_back(Type::getInt32Ty(getGlobalContext()));
							funcValues.push_back(op);
						}
						else if (opType->isPointerTy()){
							funcTypes.push_back(opType);
							funcValues.push_back(op);
						}
					}
					ArrayRef<Value*> newFuncVals(funcValues);
	//				Function* newFunc = createGate(funcTypes, funcName, M);
					string newFuncName = "llvm." + funcName;
					Function* newGate = M->getFunction(newFuncName);
					if (!newGate){
    					ArrayRef<Type*> newFuncArgsRef(funcTypes);
    					FunctionType *newFuncType = FunctionType::get(
						  				      Type::getVoidTy(getGlobalContext()),
						  				      newFuncArgsRef,
						  				      false);      
    					newGate  = Function::Create(newFuncType, 
											  GlobalVariable::ExternalLinkage,newFuncName, M); 

						newGate->addFnAttr(Attribute::NoUnwind);
					}
					BasicBlock::iterator ii(&CI);
					ReplaceInstWithInst(CI.getParent()->getInstList(), ii, CallInst::Create(newGate,newFuncVals));
				} 	
			} // visitCallInst()
		}; // struct CGVisitor

		virtual bool runOnModule(Module &M) {
			CGVisitor RV(&M);
			RV.visit(M);
			return true;
		} // runOnModule()
		
	}; // struct CreateGates
} // namespace

char CreateGates::ID = 0;
static RegisterPass<CreateGates> X("CreateGates", "RKQC Generator", false, false);

