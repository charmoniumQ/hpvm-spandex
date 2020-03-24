#ifndef __DFG2LLVM_H__
#define __DFG2LLVM_H__

//===---- DFG2LLVM.h - Header file for "HPVM Dataflow Graph to Target" ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This defines different classes for traversing Dataflow Graph for code
// generation for different nodes for different targets.
//
//===----------------------------------------------------------------------===//

#include "BuildDFG/BuildDFG.h"
#include "SupportHPVM/HPVMHint.h"
#include "SupportHPVM/HPVMTimer.h"
#include "SupportHPVM/HPVMUtils.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace builddfg;

#define TIMER(X)                                                               \
  do {                                                                         \
    if (HPVMTimer) {                                                           \
      X;                                                                       \
    }                                                                          \
  } while (0)
#define DECLARE(X)                                                             \
  X = M.getOrInsertFunction(                                                   \
      #X, runtimeModule->getFunction(#X)->getFunctionType());                  \

namespace dfg2llvm {
// Helper Functions
static inline ConstantInt *getTimerID(Module &, enum hpvm_TimerID);

bool hasAttribute(Function *, unsigned, Attribute::AttrKind);

// DFG2LLVM abstract class implementation
class DFG2LLVM : public ModulePass {
protected:
  DFG2LLVM(char ID) : ModulePass(ID) {}

  // Member variables

  // Functions

public:
  // Pure Virtual Functions
  virtual bool runOnModule(Module &M) = 0;

  // Functions
  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<BuildDFG>();
    AU.addPreserved<BuildDFG>();
  }
};

// Abstract Visitor for Code generation traversal (tree traversal for now)
class CodeGenTraversal : public DFNodeVisitor {

protected:
  // Member variables
  Module &M;
  BuildDFG &DFG;
  bool HPVMTimer = false;
  std::string TargetName = "None";

  // Map from Old function associated with DFNode to new cloned function with
  // extra index and dimension arguments. This map also serves to find out if
  // we already have an index and dim extended function copy or not (i.e.,
  // "Have we visited this function before?")
  DenseMap<DFNode *, Value *> OutputMap;

  // HPVM Runtime API
  std::unique_ptr<Module> runtimeModule;

  FunctionCallee llvm_hpvm_initializeTimerSet;
  FunctionCallee llvm_hpvm_switchToTimer;
  FunctionCallee llvm_hpvm_printTimerSet;
  GlobalVariable *TimerSet;
  GlobalVariable *GraphIDAddr;
  Instruction *InitCall;
  Instruction *CleanupCall;

  // Functions
  Value *getStringPointer(const Twine &S, Instruction *InsertBefore,
                          const Twine &Name = "");
  //  void addArgument(Function*, Type*, const Twine& Name = "");
  Function *addArgument(Function *, Type *, const Twine &Name = "");
  //  void addIdxDimArgs(Function* F);
  Function *addIdxDimArgs(Function *F);
  std::vector<Value *> extractElements(Value *, std::vector<Type *>,
                                       std::vector<std::string>, Instruction *);
  Argument *getArgumentAt(Function *F, unsigned offset);
  void initTimerAPI();

  // Pure Virtual Functions
  virtual void init() = 0;
  virtual void initRuntimeAPI() = 0;
  virtual void codeGen(DFInternalNode *N) = 0;
  virtual void codeGen(DFLeafNode *N) = 0;

  // Virtual Functions
  virtual void initializeTimerSet(Instruction *);
  virtual void switchToTimer(enum hpvm_TimerID, Instruction *);
  virtual void printTimerSet(Instruction *);

  virtual ~CodeGenTraversal() {}

public:
  // Constructor
  CodeGenTraversal(Module &_M, BuildDFG &_DFG) : M(_M), DFG(_DFG) {}

  static bool checkPreferredTarget(DFNode *N, hpvm::Target T);
  static bool preferredTargetIncludes(DFNode *N, hpvm::Target T);
  hpvm::Target getPreferredTarget(DFNode *N);

  virtual void visit(DFInternalNode *N) {
    // If code has already been generated for this internal node, skip the
    // children
    if (N->getGenFunc() != NULL)
      return;

    DEBUG(errs() << "Start: Generating Code for Node (I) - "
                 << N->getFuncPointer()->getName() << "\n");

    // Follows a bottom-up approach for code generation.
    // First generate code for all the child nodes
    for (DFGraph::children_iterator i = N->getChildGraph()->begin(),
                                    e = N->getChildGraph()->end();
         i != e; ++i) {
      DFNode *child = *i;
      child->applyDFNodeVisitor(*this);
    }
    // Generate code for this internal node now. This way all the cloned
    // functions for children exist.
    codeGen(N);
    DEBUG(errs() << "DONE: Generating Code for Node (I) - "
                 << N->getFuncPointer()->getName() << "\n");
  }

  virtual void visit(DFLeafNode *N) {
    DEBUG(errs() << "Start: Generating Code for Node (L) - "
                 << N->getFuncPointer()->getName() << "\n");
    codeGen(N);
    DEBUG(errs() << "DONE: Generating Code for Node (L) - "
                 << N->getFuncPointer()->getName() << "\n");
  }
};

// -------------- CodeGenTraversal Implementation -----------------

bool CodeGenTraversal::checkPreferredTarget(DFNode *N, hpvm::Target T) {
  Function *F = N->getFuncPointer();
  Module *M = F->getParent();
  NamedMDNode *HintNode;
  switch (T) {
  case hpvm::GPU_TARGET:
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_gpu");
    break;
  case hpvm::SPIR_TARGET:
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_spir");
    break;
  case hpvm::CUDNN_TARGET:
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_cudnn");
    break;
  case hpvm::PROMISE_TARGET:
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_promise");
    break;
  case hpvm::CPU_TARGET:
    HintNode = M->getOrInsertNamedMetadata("hpvm_hint_cpu");
    break;
  default:
    llvm_unreachable("Target Not supported yet!");
  }
  for (unsigned i = 0; i < HintNode->getNumOperands(); i++) {
    MDNode *MetaNode = HintNode->getOperand(i);
    Value *FHint =
        dyn_cast<ValueAsMetadata>(MetaNode->getOperand(0).get())->getValue();
    if (F == FHint)
      return true;
  }
  return false;
}

hpvm::Target CodeGenTraversal::getPreferredTarget(DFNode *N) {
  return hpvmUtils::getPreferredTarget(N->getFuncPointer());
}

bool CodeGenTraversal::preferredTargetIncludes(DFNode *N, hpvm::Target T) {

  Function *F = N->getFuncPointer();
  Module *M = F->getParent();
  std::vector<NamedMDNode *> HintNode;
  switch (T) {
  case hpvm::GPU_TARGET:
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_gpu"));
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_cpu_gpu"));
    break;
  case hpvm::SPIR_TARGET:
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_spir"));
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_cpu_spir"));
    break;
  case hpvm::CPU_TARGET:
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_cpu"));
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_cpu_gpu"));
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_cpu_spir"));
    break;
  case hpvm::CUDNN_TARGET:
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_cudnn"));
    break;
  case hpvm::PROMISE_TARGET:
    HintNode.push_back(M->getOrInsertNamedMetadata("hpvm_hint_promise"));
    break;
  case hpvm::CPU_OR_GPU_TARGET:
  case hpvm::CPU_OR_SPIR_TARGET:
    assert(false && "Target should be one of CPU/GPU/SPIR\n");
    break;
  default:
    llvm_unreachable("Target Not supported yet!");
  }

  for (unsigned h = 0; h < HintNode.size(); h++) {
    for (unsigned i = 0; i < HintNode[h]->getNumOperands(); i++) {
      MDNode *MetaNode = HintNode[h]->getOperand(i);
      Value *FHint =
          dyn_cast<ValueAsMetadata>(MetaNode->getOperand(0).get())->getValue();
      if (F == FHint)
        return true;
    }
  }

  return false;
}

// Generate Code for declaring a constant string [L x i8] and return a pointer
// to the start of it.
Value *CodeGenTraversal::getStringPointer(const Twine &S, Instruction *IB,
                                          const Twine &Name) {
  Constant *SConstant =
      ConstantDataArray::getString(M.getContext(), S.str(), true);
  Value *SGlobal =
      new GlobalVariable(M, SConstant->getType(), true,
                         GlobalValue::InternalLinkage, SConstant, Name);
  Value *Zero = ConstantInt::get(Type::getInt64Ty(M.getContext()), 0);
  Value *GEPArgs[] = {Zero, Zero};
  GetElementPtrInst *SPtr = GetElementPtrInst::Create(
      nullptr, SGlobal, ArrayRef<Value *>(GEPArgs, 2), Name + "Ptr", IB);
  return SPtr;
}

void renameNewArgument(Function *newF, const Twine &argName) {
  // Get Last argument in Function Arg List and rename it to given name
  Argument *lastArg = &*(newF->arg_end() - 1);
  lastArg->setName(argName);
}

// Creates a function with an additional argument of the specified type and
// name. The previous function is not deleted.
Function *CodeGenTraversal::addArgument(Function *F, Type *Ty,
                                        const Twine &name) {
  Argument *new_arg = new Argument(Ty, name);

  // Create the argument type list with added argument types
  std::vector<Type *> ArgTypes;
  for (Function::const_arg_iterator ai = F->arg_begin(), ae = F->arg_end();
       ai != ae; ++ai) {
    ArgTypes.push_back(ai->getType());
  }
  ArgTypes.push_back(new_arg->getType());

  // Adding new arguments to the function argument list, would not change the
  // function type. We need to change the type of this function to reflect the
  // added arguments. So, we create a clone of this function with the correct
  // type.
  FunctionType *FTy =
      FunctionType::get(F->getReturnType(), ArgTypes, F->isVarArg());
  Function *newF = Function::Create(FTy, F->getLinkage(),
                                    F->getName() + "_cloned", F->getParent());
  renameNewArgument(newF, name);
  newF = hpvmUtils::cloneFunction(F, newF, false);

  // Check if the function is used by a metadata node
  if (F->isUsedByMetadata()) {
    hpvmUtils::fixHintMetadata(*F->getParent(), F, newF);
  }

  return newF;
}

// Return new function with additional index and limit arguments.
// The original function is removed from the module and erased.
Function *CodeGenTraversal::addIdxDimArgs(Function *F) {
  DEBUG(errs() << "Function Type: " << *F->getFunctionType() << "\n");
  // Add Index and Dim arguments
  std::string names[] = {"idx_x", "idx_y", "idx_z", "dim_x", "dim_y", "dim_z"};
  Function *newF;
  for (int i = 0; i < 6; ++i) {
    newF = addArgument(F, Type::getInt64Ty(F->getContext()), names[i]);
    F->replaceAllUsesWith(UndefValue::get(F->getType()));
    F->eraseFromParent();
    F = newF;
  }
  DEBUG(errs() << "Function Type after adding args: "
               << *newF->getFunctionType() << "\n");
  return newF;
}

// Extract elements from an aggregate value. TyList contains the type of each
// element, and names vector contains a name. IB is the instruction before which
// all the generated code would be inserted.
std::vector<Value *>
CodeGenTraversal::extractElements(Value *Aggregate, std::vector<Type *> TyList,
                                  std::vector<std::string> names,
                                  Instruction *IB) {
  // Extract input data from i8* Aggregate.addr and store them in a vector.
  // For each argument
  std::vector<Value *> Elements;
  GetElementPtrInst *GEP;
  unsigned argNum = 0;
  for (Type *Ty : TyList) {
    // BitCast: %arg.addr = bitcast i8* Aggregate.addr to <pointer-to-argType>
    CastInst *BI = BitCastInst::CreatePointerCast(Aggregate, Ty->getPointerTo(),
                                                  names[argNum] + ".addr", IB);
    // Load: %arg = load <pointer-to-argType> %arg.addr
    LoadInst *LI = new LoadInst(BI, names[argNum], IB);
    // Patch argument to call instruction
    Elements.push_back(LI);
    // errs() << "Pushing element " << *LI << "\n";
    // CI->setArgOperand(argNum, LI);

    // TODO: Minor Optimization - The last GEP statement can/should be left out
    // as no more arguments left
    // Increment using GEP: %nextArg = getelementptr <ptr-to-argType> %arg.addr,
    // i64 1 This essentially takes us to the next argument in memory
    Constant *IntOne = ConstantInt::get(Type::getInt64Ty(M.getContext()), 1);
    if (argNum < TyList.size() - 1)
      GEP = GetElementPtrInst::Create(nullptr, BI, ArrayRef<Value *>(IntOne),
                                      "nextArg", IB);
    // Increment argNum and for the next iteration use result of this GEP to
    // extract next argument
    argNum++;
    Aggregate = GEP;
  }
  return Elements;
}

// Traverse the function F argument list to get argument at offset
Argument *CodeGenTraversal::getArgumentAt(Function *F, unsigned offset) {
  DEBUG(errs() << "Finding argument " << offset << ":\n");
  assert((F->getFunctionType()->getNumParams() > offset) &&
         "Invalid offset to access arguments!");

  Function::arg_iterator ArgIt = F->arg_begin() + offset;
  Argument *Arg = &*ArgIt;
  return Arg;
}

void CodeGenTraversal::initTimerAPI() {
  DECLARE(llvm_hpvm_initializeTimerSet);
  DECLARE(llvm_hpvm_switchToTimer);
  DECLARE(llvm_hpvm_printTimerSet);
}

// Timer Routines
// Initialize the timer set
void CodeGenTraversal::initializeTimerSet(Instruction *InsertBefore) {
  // DEBUG(errs() << "Inserting call to: " << *llvm_hpvm_initializeTimerSet <<
  // "\n");
  TIMER(TimerSet = new GlobalVariable(
            M, Type::getInt8PtrTy(M.getContext()), false,
            GlobalValue::CommonLinkage,
            Constant::getNullValue(Type::getInt8PtrTy(M.getContext())),
            Twine("hpvmTimerSet_") + TargetName);
        DEBUG(errs() << "New global variable: " << *TimerSet << "\n");

        Value *TimerSetAddr = CallInst::Create(llvm_hpvm_initializeTimerSet,
                                               None, "", InsertBefore);
        new StoreInst(TimerSetAddr, TimerSet, InsertBefore););
}

void CodeGenTraversal::switchToTimer(enum hpvm_TimerID timer,
                                     Instruction *InsertBefore) {
  Value *switchArgs[] = {TimerSet, getTimerID(M, timer)};
  TIMER(CallInst::Create(llvm_hpvm_switchToTimer,
                         ArrayRef<Value *>(switchArgs, 2), "", InsertBefore));
}

void CodeGenTraversal::printTimerSet(Instruction *InsertBefore) {
  Value *TimerName;
  TIMER(TimerName =
            getStringPointer(TargetName + Twine("_Timer"), InsertBefore));
  Value *printArgs[] = {TimerSet, TimerName};
  TIMER(CallInst::Create(llvm_hpvm_printTimerSet,
                         ArrayRef<Value *>(printArgs, 2), "", InsertBefore));
}

// Implementation of Helper Functions
static inline ConstantInt *getTimerID(Module &M, enum hpvm_TimerID timer) {
  return ConstantInt::get(Type::getInt32Ty(M.getContext()), timer);
}

static inline ConstantInt *getTargetID(Module &M, enum hpvm::Target T) {
  return ConstantInt::get(Type::getInt32Ty(M.getContext()), T);
}

// Find if argument has the given attribute
bool hasAttribute(Function *F, unsigned arg_index, Attribute::AttrKind AK) {
  return F->getAttributes().hasAttribute(arg_index + 1, AK);
}

} // namespace dfg2llvm

#endif
