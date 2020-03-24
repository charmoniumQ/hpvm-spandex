//==------- GenHPVM.h - Header file for "LLVM IR to HPVM IR Pass" ----------==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the GenHPVM pass responsible for converting HPVM-C to
// HPVM intrinsics. Note that this pass relies on memory-to-register optimiza-
// tion pass to execute before this executes.
//
//===----------------------------------------------------------------------===//

#include "SupportHPVM/HPVMTimer.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace genhpvm {
// GenHPVM - The first implementation.
struct GenHPVM : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  GenHPVM() : ModulePass(ID) {}

private:
  // Member variables
  Module *M;
  FunctionCallee llvm_hpvm_initializeTimerSet;
  FunctionCallee llvm_hpvm_switchToTimer;
  FunctionCallee llvm_hpvm_printTimerSet;

  GlobalVariable *TimerSet;

  // Functions
  void initializeTimerSet(Instruction *);
  void switchToTimer(enum hpvm_TimerID, Instruction *);
  void printTimerSet(Instruction *);
  Value *getStringPointer(const Twine &S, Instruction *InsertBefore,
                          const Twine &Name = "");

public:
  // Functions
  virtual bool runOnModule(Module &M);
};

} // namespace genhpvm
