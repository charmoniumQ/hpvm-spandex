//=== GenHPVM.cpp - Implements "Hierarchical Dataflow Graph Builder Pass" ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass takes LLVM IR with HPVM-C functions to generate textual representa-
// -tion for HPVM IR consisting of HPVM intrinsics. Memory-to-register
// optimization pass is expected to execute prior to execution of this pass.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "genhpvm"
#include "GenHPVM/GenHPVM.h"

#include "SupportHPVM/HPVMHint.h"
#include "SupportHPVM/HPVMUtils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#define TIMER(X)                                                               \
  do {                                                                         \
    if (HPVMTimer) {                                                           \
      X;                                                                       \
    }                                                                          \
  } while (0)

#ifndef LLVM_BUILD_DIR
#error LLVM_BUILD_DIR is not defined
#endif

#define STR_VALUE(X) #X
#define STRINGIFY(X) STR_VALUE(X)
#define LLVM_BUILD_DIR_STR STRINGIFY(LLVM_BUILD_DIR)

using namespace llvm;
using namespace hpvmUtils;

// HPVM Command line option to use timer or not
static cl::opt<bool> HPVMTimer("hpvm-timers-gen",
                               cl::desc("Enable GenHPVM timer"));

namespace genhpvm {

// Helper Functions

static inline ConstantInt *getTimerID(Module &, enum hpvm_TimerID);
static Function *transformReturnTypeToStruct(Function *F);
static Type *getReturnTypeFromReturnInst(Function *F);

// Check if the dummy function call is a __hpvm__node call
#define IS_HPVM_CALL(callName)                                                 \
  static bool isHPVMCall_##callName(Instruction *I) {                          \
    if (!isa<CallInst>(I))                                                     \
      return false;                                                            \
    CallInst *CI = cast<CallInst>(I);                                          \
    return (CI->getCalledValue()->stripPointerCasts()->getName())              \
        .equals("__hpvm__" #callName);                                         \
  }

static void ReplaceCallWithIntrinsic(Instruction *I, Intrinsic::ID IntrinsicID,
                                     std::vector<Instruction *> *Erase) {
  // Check if the instruction is Call Instruction
  assert(isa<CallInst>(I) && "Expecting CallInst");
  CallInst *CI = cast<CallInst>(I);
  DEBUG(errs() << "Found call: " << *CI << "\n");

  // Find the correct intrinsic call
  Module *M = CI->getParent()->getParent()->getParent();
  Function *F;
  std::vector<Type *> ArgTypes;
  std::vector<Value *> args;
  if (Intrinsic::isOverloaded(IntrinsicID)) {
    // This is an overloaded intrinsic. The types must exactly match. Get the
    // argument types
    for (unsigned i = 0; i < CI->getNumArgOperands(); i++) {
      ArgTypes.push_back(CI->getArgOperand(i)->getType());
      args.push_back(CI->getArgOperand(i));
    }
    F = Intrinsic::getDeclaration(M, IntrinsicID, ArgTypes);
    DEBUG(errs() << *F << "\n");
  } else { // Non-overloaded intrinsic
    F = Intrinsic::getDeclaration(M, IntrinsicID);
    FunctionType *FTy = F->getFunctionType();
    DEBUG(errs() << *F << "\n");

    // Create argument list
    assert(CI->getNumArgOperands() == FTy->getNumParams() &&
           "Number of arguments of call do not match with Intrinsic");
    for (unsigned i = 0; i < CI->getNumArgOperands(); i++) {
      Value *V = CI->getArgOperand(i);
      // Either the type should match or both should be of pointer type
      assert((V->getType() == FTy->getParamType(i) ||
              (V->getType()->isPointerTy() &&
               FTy->getParamType(i)->isPointerTy())) &&
             "Dummy function call argument does not match with Intrinsic "
             "argument!");
      // If the types do not match, then both must be pointer type and pointer
      // cast needs to be performed
      if (V->getType() != FTy->getParamType(i)) {
        V = CastInst::CreatePointerCast(V, FTy->getParamType(i), "", CI);
      }
      args.push_back(V);
    }
  }
  // Insert call instruction
  CallInst *Inst = CallInst::Create(
      F, args, F->getReturnType()->isVoidTy() ? "" : CI->getName(), CI);

  DEBUG(errs() << "\tSubstitute with: " << *Inst << "\n");

  CI->replaceAllUsesWith(Inst);
  // If the previous instruction needs to be erased, insert it in the vector
  // Erased
  if (Erase != NULL)
    Erase->push_back(CI);
}

IS_HPVM_CALL(launch) /* Exists but not required */
IS_HPVM_CALL(edge)   /* Exists but not required */
IS_HPVM_CALL(createNodeND)
IS_HPVM_CALL(bindIn)
IS_HPVM_CALL(bindOut)
IS_HPVM_CALL(push)
IS_HPVM_CALL(pop)
IS_HPVM_CALL(getNode)
IS_HPVM_CALL(getParentNode)
IS_HPVM_CALL(barrier)
IS_HPVM_CALL(malloc)
IS_HPVM_CALL(return )
IS_HPVM_CALL(getNodeInstanceID_x)
IS_HPVM_CALL(getNodeInstanceID_y)
IS_HPVM_CALL(getNodeInstanceID_z)
IS_HPVM_CALL(getNumNodeInstances_x)
IS_HPVM_CALL(getNumNodeInstances_y)
IS_HPVM_CALL(getNumNodeInstances_z)
// Atomics
IS_HPVM_CALL(atomic_add)
IS_HPVM_CALL(atomic_sub)
IS_HPVM_CALL(atomic_xchg)
IS_HPVM_CALL(atomic_min)
IS_HPVM_CALL(atomic_max)
IS_HPVM_CALL(atomic_and)
IS_HPVM_CALL(atomic_or)
IS_HPVM_CALL(atomic_xor)
// Misc Fn
IS_HPVM_CALL(sin)
IS_HPVM_CALL(cos)

IS_HPVM_CALL(init)
IS_HPVM_CALL(cleanup)
IS_HPVM_CALL(wait)
IS_HPVM_CALL(trackMemory)
IS_HPVM_CALL(untrackMemory)
IS_HPVM_CALL(requestMemory)
IS_HPVM_CALL(attributes)
IS_HPVM_CALL(hint)

// Return the constant integer represented by value V
static unsigned getNumericValue(Value *V) {
  assert(isa<ConstantInt>(V) &&
      "Value indicating the number of arguments should be a constant integer");
  return cast<ConstantInt>(V)->getZExtValue();
}

// Take the __hpvm__return instruction and generate code for combining the
// values being returned into a struct and returning it.
// The first operand is the number of returned values
static Value *genCodeForReturn(CallInst *CI) {
  LLVMContext &Ctx = CI->getContext();
  assert(isHPVMCall_return(CI) && "__hpvm__return instruction expected!");

  // Parse the dummy function call here
  assert(CI->getNumArgOperands() > 0 &&
         "Too few arguments for __hpvm_return call!\n");
  unsigned numRetVals = getNumericValue(CI->getArgOperand(0));

  assert(CI->getNumArgOperands() - 1 == numRetVals &&
         "Too few arguments for __hpvm_return call!\n");
  DEBUG(errs() << "\tNum of return values = " << numRetVals << "\n");

  std::vector<Type *> ArgTypes;
  for (unsigned i = 1; i < CI->getNumArgOperands(); i++) {
    ArgTypes.push_back(CI->getArgOperand(i)->getType());
  }
  Twine outTyName = "struct.out." + CI->getParent()->getParent()->getName();
  StructType *RetTy = StructType::create(Ctx, ArgTypes, outTyName.str(), true);

  InsertValueInst *IV = InsertValueInst::Create(
      UndefValue::get(RetTy), CI->getArgOperand(1), 0, "returnStruct", CI);
  DEBUG(errs() << "Code generation for return:\n");
  DEBUG(errs() << *IV << "\n");

  for (unsigned i = 2; i < CI->getNumArgOperands(); i++) {
    IV = InsertValueInst::Create(IV, CI->getArgOperand(i), i - 1, IV->getName(),
                                 CI);
    DEBUG(errs() << *IV << "\n");
  }

  return IV;
}

// Analyse the attribute call for this function. Add the in and out
// attributes to pointer parameters.
static void handleHPVMAttributes(Function *F, CallInst *CI) {
  DEBUG(errs() << "Kernel before adding In/Out HPVM attributes:\n"
               << *F << "\n");
  // Parse the dummy function call here
  unsigned offset = 0;
  // Find number of In pointers
  assert(CI->getNumArgOperands() > offset &&
         "Too few arguments for __hpvm__attributes call!");
  unsigned numInPtrs = getNumericValue(CI->getArgOperand(offset));
  DEBUG(errs() << "\tNum of in pointers = " << numInPtrs << "\n");

  for (unsigned i = offset + 1; i < offset + 1 + numInPtrs; i++) {
    Value *V = CI->getArgOperand(i);
    if (Argument *arg = dyn_cast<Argument>(V)) {
      F->addAttribute(1 + arg->getArgNo(), Attribute::In);
    } else {
      DEBUG(errs() << "Invalid argument to __hpvm__attribute: " << *V << "\n");
      llvm_unreachable(
          "Only pointer arguments can be passed to __hpvm__attributes call");
    }
  }
  // Find number of Out Pointers
  offset += 1 + numInPtrs;
  assert(CI->getNumArgOperands() > offset &&
         "Too few arguments for __hpvm__attributes call!");
  unsigned numOutPtrs = getNumericValue(CI->getOperand(offset));
  DEBUG(errs() << "\tNum of out Pointers = " << numOutPtrs << "\n");
  for (unsigned i = offset + 1; i < offset + 1 + numOutPtrs; i++) {
    Value *V = CI->getArgOperand(i);
    if (Argument *arg = dyn_cast<Argument>(V)) {
      F->addAttribute(1 + arg->getArgNo(), Attribute::Out);
    } else {
      DEBUG(errs() << "Invalid argument to __hpvm__attribute: " << *V << "\n");
      llvm_unreachable(
          "Only pointer arguments can be passed to __hpvm__attributes call");
    }
  }
  DEBUG(errs() << "Kernel after adding In/Out HPVM attributes:\n"
               << *F << "\n");
}

// Public Functions of GenHPVM pass
bool GenHPVM::runOnModule(Module &M) {
  DEBUG(errs() << "\nGENHPVM PASS\n");
  this->M = &M;

  // Load Runtime API Module
  SMDiagnostic Err;

  std::string runtimeAPI = std::string(LLVM_BUILD_DIR_STR) +
                           "/tools/hpvm/projects/hpvm-rt/hpvm-rt.bc";

  std::unique_ptr<Module> runtimeModule =
      parseIRFile(runtimeAPI, Err, M.getContext());

  if (runtimeModule == NULL) {
    DEBUG(errs() << Err.getMessage() << " " << runtimeAPI << "\n");
    assert(false && "couldn't parse runtime");
  } else
    DEBUG(errs() << "Successfully loaded hpvm-rt API module\n");

  llvm_hpvm_initializeTimerSet = M.getOrInsertFunction(
      "llvm_hpvm_initializeTimerSet",
      runtimeModule->getFunction("llvm_hpvm_initializeTimerSet")
          ->getFunctionType());
  // DEBUG(errs() << *llvm_hpvm_initializeTimerSet);

  llvm_hpvm_switchToTimer = M.getOrInsertFunction(
      "llvm_hpvm_switchToTimer",
      runtimeModule->getFunction("llvm_hpvm_switchToTimer")->getFunctionType());
  // DEBUG(errs() << *llvm_hpvm_switchToTimer);

  llvm_hpvm_printTimerSet = M.getOrInsertFunction(
      "llvm_hpvm_printTimerSet",
      runtimeModule->getFunction("llvm_hpvm_printTimerSet")->getFunctionType());
  // DEBUG(errs() << *llvm_hpvm_printTimerSet);

  // Insert init context in main
  DEBUG(errs() << "Locate __hpvm__init()\n");
  Function *VI = M.getFunction("__hpvm__init");
  assert(VI->getNumUses() == 1 && "__hpvm__init should only be used once");
  Instruction *I = cast<Instruction>(*VI->user_begin());

  DEBUG(errs() << "Initialize Timer Set\n");
  initializeTimerSet(I);
  switchToTimer(hpvm_TimerID_NONE, I);

  // Insert print instruction at hpvm exit
  DEBUG(errs() << "Locate __hpvm__cleanup()\n");
  Function *VC = M.getFunction("__hpvm__cleanup");
  assert(VC->getNumUses() == 1 && "__hpvm__cleanup should only be used once");
  I = cast<Instruction>(*VC->user_begin());
  printTimerSet(I);

  DEBUG(errs() << "-------- Searching for launch sites ----------\n");

  std::vector<Instruction *> toBeErased;
  std::vector<Function *> functions;

  for (auto &F : M)
    functions.push_back(&F);

  // Iterate over all functions in the module
  for (Function *f : functions) {
    DEBUG(errs() << "Function: " << f->getName() << "\n");

    // List with the required additions in the function's return type
    std::vector<Type *> FRetTypes;

    enum mutateTypeCause {
      mtc_None,
      mtc_BIND,
      mtc_RETURN,
      mtc_NUM_CAUSES
    } bind;
    bind = mutateTypeCause::mtc_None;

    // Iterate over all the instructions in this function
    for (inst_iterator i = inst_begin(f), e = inst_end(f); i != e; ++i) {
      Instruction *I = &*i; // Grab pointer to Instruction
      // If not a call instruction, move to next instruction
      if (!isa<CallInst>(I))
        continue;

      CallInst *CI = cast<CallInst>(I);
      LLVMContext &Ctx = CI->getContext();

      if (isHPVMCall_init(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_init, &toBeErased);
      }
      if (isHPVMCall_cleanup(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_cleanup, &toBeErased);
      }
      if (isHPVMCall_wait(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_wait, &toBeErased);
      }
      if (isHPVMCall_trackMemory(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_trackMemory, &toBeErased);
      }
      if (isHPVMCall_untrackMemory(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_untrackMemory, &toBeErased);
      }
      if (isHPVMCall_requestMemory(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_requestMemory, &toBeErased);
      }
      if (isHPVMCall_hint(I)) {
        assert(isa<ConstantInt>(CI->getArgOperand(0)) &&
               "Argument to hint must be constant integer!");
        ConstantInt *hint = cast<ConstantInt>(CI->getArgOperand(0));

        hpvm::Target t = (hpvm::Target)hint->getZExtValue();
        addHint(CI->getParent()->getParent(), t);
        DEBUG(errs() << "Found hpvm hint call: " << *CI << "\n");
        toBeErased.push_back(CI);
      }
      if (isHPVMCall_launch(I)) {
        Function *LaunchF =
            Intrinsic::getDeclaration(&M, Intrinsic::hpvm_launch);
        DEBUG(errs() << *LaunchF << "\n");
        // Get i8* cast to function pointer
        Function *graphFunc = cast<Function>(CI->getArgOperand(1));
        graphFunc = transformReturnTypeToStruct(graphFunc);
        Constant *F =
            ConstantExpr::getPointerCast(graphFunc, Type::getInt8PtrTy(Ctx));
        assert(
            F &&
            "Function invoked by HPVM launch has to be define and constant.");

        ConstantInt *Op = cast<ConstantInt>(CI->getArgOperand(0));
        assert(Op && "HPVM launch's streaming argument is a constant value.");
        Value *isStreaming = Op->isZero() ? ConstantInt::getFalse(Ctx)
                                          : ConstantInt::getTrue(Ctx);

        auto *ArgTy = dyn_cast<PointerType>(CI->getArgOperand(2)->getType());
        assert(ArgTy && "HPVM launch argument should be pointer type.");
        Value *Arg = CI->getArgOperand(2);
        if (!ArgTy->getElementType()->isIntegerTy(8))
          Arg = BitCastInst::CreatePointerCast(CI->getArgOperand(2),
                                               Type::getInt8PtrTy(Ctx), "", CI);
        Value *LaunchArgs[] = {F, Arg, isStreaming};
        CallInst *LaunchInst = CallInst::Create(
            LaunchF, ArrayRef<Value *>(LaunchArgs, 3), "graphID", CI);
        DEBUG(errs() << "Found hpvm launch call: " << *CI << "\n");
        DEBUG(errs() << "\tSubstitute with: " << *LaunchInst << "\n");
        CI->replaceAllUsesWith(LaunchInst);
        toBeErased.push_back(CI);
      }
      if (isHPVMCall_push(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_push, &toBeErased);
      }
      if (isHPVMCall_pop(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_pop, &toBeErased);
      }
      if (isHPVMCall_createNodeND(I)) {
        assert(CI->getNumArgOperands() > 0 &&
               "Too few arguments for __hpvm__createNodeND call");
        unsigned numDims = getNumericValue(CI->getArgOperand(0));
        // We need as meny dimension argments are there are dimensions
        assert(CI->getNumArgOperands() - 2 == numDims &&
               "Too few arguments for __hpvm_createNodeND call!\n");

        Function *CreateNodeF;
        switch (numDims) {
        case 0:
          CreateNodeF =
              Intrinsic::getDeclaration(&M, Intrinsic::hpvm_createNode);
          break;
        case 1:
          CreateNodeF =
              Intrinsic::getDeclaration(&M, Intrinsic::hpvm_createNode1D);
          break;
        case 2:
          CreateNodeF =
              Intrinsic::getDeclaration(&M, Intrinsic::hpvm_createNode2D);
          break;
        case 3:
          CreateNodeF =
              Intrinsic::getDeclaration(&M, Intrinsic::hpvm_createNode3D);
          break;
        default:
          llvm_unreachable("Unsupported number of dimensions\n");
          break;
        }
        DEBUG(errs() << *CreateNodeF << "\n");
        DEBUG(errs() << *I << "\n");
        DEBUG(errs() << "in " << I->getParent()->getParent()->getName()
                     << "\n");

        // Get i8* cast to function pointer
        Function *graphFunc = cast<Function>(CI->getArgOperand(1));
        graphFunc = transformReturnTypeToStruct(graphFunc);
        Constant *F =
            ConstantExpr::getPointerCast(graphFunc, Type::getInt8PtrTy(Ctx));

        CallInst *CreateNodeInst;
        switch (numDims) {
        case 0:
          CreateNodeInst = CallInst::Create(CreateNodeF, ArrayRef<Value *>(F),
                                            graphFunc->getName() + ".node", CI);
          break;
        case 1: {
          assert((CI->getArgOperand(2)->getType() == Type::getInt64Ty(Ctx)) &&
                 "CreateNodeND dimension argument, 2, expected to be i64\n");
          Value *CreateNodeArgs[] = {F, CI->getArgOperand(2)};
          CreateNodeInst = CallInst::Create(
              CreateNodeF, ArrayRef<Value *>(CreateNodeArgs, 2),
              graphFunc->getName() + ".node", CI);
        } break;
        case 2: {
          assert((CI->getArgOperand(2)->getType() == Type::getInt64Ty(Ctx)) &&
                 "CreateNodeND dimension argument, 2, expected to be i64\n");
          assert((CI->getArgOperand(3)->getType() == Type::getInt64Ty(Ctx)) &&
                 "CreateNodeND dimension argument, 3, expected to be i64\n");
          Value *CreateNodeArgs[] = {F, CI->getArgOperand(2),
                                     CI->getArgOperand(3)};
          CreateNodeInst = CallInst::Create(
              CreateNodeF, ArrayRef<Value *>(CreateNodeArgs, 3),
              graphFunc->getName() + ".node", CI);
        } break;
        case 3: {
          assert((CI->getArgOperand(2)->getType() == Type::getInt64Ty(Ctx)) &&
                 "CreateNodeND dimension argument, 2, expected to be i64\n");
          assert((CI->getArgOperand(3)->getType() == Type::getInt64Ty(Ctx)) &&
                 "CreateNodeND dimension argument, 3, expected to be i64\n");
          assert((CI->getArgOperand(4)->getType() == Type::getInt64Ty(Ctx)) &&
                 "CreateNodeND dimension argument, 4, expected to be i64\n");
          Value *CreateNodeArgs[] = {F, CI->getArgOperand(2),
                                     CI->getArgOperand(3),
                                     CI->getArgOperand(4)};
          CreateNodeInst = CallInst::Create(
              CreateNodeF, ArrayRef<Value *>(CreateNodeArgs, 4),
              graphFunc->getName() + ".node", CI);
        } break;
        default:
          llvm_unreachable(
              "Impossible path: number of dimensions is 0, 1, 2, 3\n");
          break;
        }

        DEBUG(errs() << "Found hpvm createNode call: " << *CI << "\n");
        DEBUG(errs() << "\tSubstitute with: " << *CreateNodeInst << "\n");
        CI->replaceAllUsesWith(CreateNodeInst);
        toBeErased.push_back(CI);
      }

      if (isHPVMCall_edge(I)) {
        Function *EdgeF =
            Intrinsic::getDeclaration(&M, Intrinsic::hpvm_createEdge);
        DEBUG(errs() << *EdgeF << "\n");
        ConstantInt *Op = cast<ConstantInt>(CI->getArgOperand(5));
        ConstantInt *EdgeTypeOp = cast<ConstantInt>(CI->getArgOperand(2));
        assert(Op && EdgeTypeOp &&
               "Arguments of CreateEdge are not constant integers.");
        Value *isStreaming = Op->isZero() ? ConstantInt::getFalse(Ctx)
                                          : ConstantInt::getTrue(Ctx);
        Value *isAllToAll = EdgeTypeOp->isZero() ? ConstantInt::getFalse(Ctx)
                                                 : ConstantInt::getTrue(Ctx);
        Value *EdgeArgs[] = {CI->getArgOperand(0), CI->getArgOperand(1),
                             isAllToAll,           CI->getArgOperand(3),
                             CI->getArgOperand(4), isStreaming};
        CallInst *EdgeInst = CallInst::Create(
            EdgeF, ArrayRef<Value *>(EdgeArgs, 6), "output", CI);
        DEBUG(errs() << "Found hpvm edge call: " << *CI << "\n");
        DEBUG(errs() << "\tSubstitute with: " << *EdgeInst << "\n");
        CI->replaceAllUsesWith(EdgeInst);
        toBeErased.push_back(CI);
      }
      if (isHPVMCall_bindIn(I)) {
        Function *BindInF =
            Intrinsic::getDeclaration(&M, Intrinsic::hpvm_bind_input);
        DEBUG(errs() << *BindInF << "\n");
        // Check if this is a streaming bind or not
        ConstantInt *Op = cast<ConstantInt>(CI->getArgOperand(3));
        assert(Op && "Streaming argument for bind in intrinsic should be a "
                     "constant integer.");
        Value *isStreaming = Op->isZero() ? ConstantInt::getFalse(Ctx)
                                          : ConstantInt::getTrue(Ctx);
        Value *BindInArgs[] = {CI->getArgOperand(0), CI->getArgOperand(1),
                               CI->getArgOperand(2), isStreaming};
        CallInst *BindInInst =
            CallInst::Create(BindInF, ArrayRef<Value *>(BindInArgs, 4), "", CI);
        DEBUG(errs() << "Found hpvm bindIn call: " << *CI << "\n");
        DEBUG(errs() << "\tSubstitute with: " << *BindInInst << "\n");
        CI->replaceAllUsesWith(BindInInst);
        toBeErased.push_back(CI);
      }
      if (isHPVMCall_bindOut(I)) {
        Function *BindOutF =
            Intrinsic::getDeclaration(&M, Intrinsic::hpvm_bind_output);
        DEBUG(errs() << *BindOutF << "\n");
        // Check if this is a streaming bind or not
        ConstantInt *Op = cast<ConstantInt>(CI->getArgOperand(3));
        assert(Op && "Streaming argument for bind out intrinsic should be a "
                     "constant integer.");
        Value *isStreaming = Op->isZero() ? ConstantInt::getFalse(Ctx)
                                          : ConstantInt::getTrue(Ctx);
        Value *BindOutArgs[] = {CI->getArgOperand(0), CI->getArgOperand(1),
                                CI->getArgOperand(2), isStreaming};
        CallInst *BindOutInst = CallInst::Create(
            BindOutF, ArrayRef<Value *>(BindOutArgs, 4), "", CI);
        DEBUG(errs() << "Found hpvm bindOut call: " << *CI << "\n");
        DEBUG(errs() << "\tSubstitute with: " << *BindOutInst << "\n");

        DEBUG(errs() << "Fixing the return type of the function\n");
        // FIXME: What if the child node function has not been visited already.
        // i.e., it's return type has not been fixed.
        Function *F = I->getParent()->getParent();
        DEBUG(errs() << F->getName() << "\n";);
        IntrinsicInst *NodeIntrinsic =
            cast<IntrinsicInst>(CI->getArgOperand(0));
        assert(NodeIntrinsic &&
               "Instruction value in bind out is not a create node intrinsic.");
        DEBUG(errs() << "Node intrinsic: " << *NodeIntrinsic << "\n");
        assert(
            (NodeIntrinsic->getIntrinsicID() == Intrinsic::hpvm_createNode ||
             NodeIntrinsic->getIntrinsicID() == Intrinsic::hpvm_createNode1D ||
             NodeIntrinsic->getIntrinsicID() == Intrinsic::hpvm_createNode2D ||
             NodeIntrinsic->getIntrinsicID() == Intrinsic::hpvm_createNode3D) &&
            "Instruction value in bind out is not a create node intrinsic.");
        Function *ChildF = cast<Function>(
            NodeIntrinsic->getArgOperand(0)->stripPointerCasts());
        DEBUG(errs() << ChildF->getName() << "\n";);
        int srcpos = cast<ConstantInt>(CI->getArgOperand(1))->getSExtValue();
        int destpos = cast<ConstantInt>(CI->getArgOperand(2))->getSExtValue();
        StructType *ChildReturnTy = cast<StructType>(ChildF->getReturnType());

        Type *ReturnType = F->getReturnType();
        DEBUG(errs() << *ReturnType << "\n";);
        assert((ReturnType->isVoidTy() || isa<StructType>(ReturnType)) &&
               "Return type should either be a struct or void type!");

        FRetTypes.insert(FRetTypes.begin() + destpos,
                         ChildReturnTy->getElementType(srcpos));
        assert(((bind == mutateTypeCause::mtc_BIND) ||
                (bind == mutateTypeCause::mtc_None)) &&
               "Both bind_out and hpvm_return detected");
        bind = mutateTypeCause::mtc_BIND;

        CI->replaceAllUsesWith(BindOutInst);
        toBeErased.push_back(CI);
      }
      if (isHPVMCall_attributes(I)) {
        Function *F = CI->getParent()->getParent();
        handleHPVMAttributes(F, CI);
        toBeErased.push_back(CI);
      }
      if (isHPVMCall_getNode(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_getNode, &toBeErased);
      }
      if (isHPVMCall_getParentNode(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_getParentNode, &toBeErased);
      }
      if (isHPVMCall_barrier(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_barrier, &toBeErased);
      }
      if (isHPVMCall_malloc(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_malloc, &toBeErased);
      }
      if (isHPVMCall_return(I)) {
        DEBUG(errs() << "Function before hpvm return processing\n"
                     << *I->getParent()->getParent() << "\n");
        // The operands to this call are the values to be returned by the node
        Value *ReturnVal = genCodeForReturn(CI);
        DEBUG(errs() << *ReturnVal << "\n");
        Type *ReturnType = ReturnVal->getType();
        assert(isa<StructType>(ReturnType) &&
               "Return type should be a struct type!");

        assert(((bind == mutateTypeCause::mtc_RETURN) ||
                (bind == mutateTypeCause::mtc_None)) &&
               "Both bind_out and hpvm_return detected");

        if (bind == mutateTypeCause::mtc_None) {
          // If this is None, this is the first __hpvm__return
          // instruction we have come upon. Place the return type of the
          // function in the return type vector
          bind = mutateTypeCause::mtc_RETURN;
          StructType *ReturnStructTy = cast<StructType>(ReturnType);
          for (unsigned i = 0; i < ReturnStructTy->getNumElements(); i++)
            FRetTypes.push_back(ReturnStructTy->getElementType(i));
        } else { // bind == mutateTypeCause::mtc_RETURN
          // This is not the first __hpvm__return
          // instruction we have come upon.
          // Check that the return types are the same
          assert((ReturnType == FRetTypes[0]) &&
                 "Multiple returns with mismatching types");
        }

        ReturnInst *RetInst = ReturnInst::Create(Ctx, ReturnVal);
        DEBUG(errs() << "Found hpvm return call: " << *CI << "\n");
        Instruction *oldReturn = CI->getParent()->getTerminator();
        assert(isa<ReturnInst>(oldReturn) &&
               "Expecting a return to be the terminator of this BB!");
        DEBUG(errs() << "Found return statement of BB: " << *oldReturn << "\n");
        DEBUG(errs() << "\tSubstitute return with: " << *RetInst << "\n");
        // CI->replaceAllUsesWith(RetInst);
        toBeErased.push_back(CI);
        ReplaceInstWithInst(oldReturn, RetInst);
        DEBUG(errs() << "Function after hpvm return processing\n"
                     << *I->getParent()->getParent() << "\n");
      }

      if (isHPVMCall_getNodeInstanceID_x(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_getNodeInstanceID_x,
                                 &toBeErased);
      }
      if (isHPVMCall_getNodeInstanceID_y(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_getNodeInstanceID_y,
                                 &toBeErased);
      }
      if (isHPVMCall_getNodeInstanceID_z(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_getNodeInstanceID_z,
                                 &toBeErased);
      }
      if (isHPVMCall_getNumNodeInstances_x(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_getNumNodeInstances_x,
                                 &toBeErased);
      }
      if (isHPVMCall_getNumNodeInstances_y(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_getNumNodeInstances_y,
                                 &toBeErased);
      }
      if (isHPVMCall_getNumNodeInstances_z(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_getNumNodeInstances_z,
                                 &toBeErased);
      }
      if (isHPVMCall_atomic_add(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_atomic_add, &toBeErased);
      }
      if (isHPVMCall_atomic_sub(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_atomic_sub, &toBeErased);
      }
      if (isHPVMCall_atomic_xchg(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_atomic_xchg, &toBeErased);
      }
      if (isHPVMCall_atomic_min(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_atomic_min, &toBeErased);
      }
      if (isHPVMCall_atomic_max(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_atomic_max, &toBeErased);
      }
      if (isHPVMCall_atomic_and(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_atomic_and, &toBeErased);
      }
      if (isHPVMCall_atomic_or(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_atomic_or, &toBeErased);
      }
      if (isHPVMCall_atomic_xor(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::hpvm_atomic_xor, &toBeErased);
      }
      if (isHPVMCall_sin(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::sin, &toBeErased);
      }
      if (isHPVMCall_cos(I)) {
        ReplaceCallWithIntrinsic(I, Intrinsic::cos, &toBeErased);
      }
    }

    // Erase the __hpvm__node calls
    DEBUG(errs() << "Erase " << toBeErased.size() << " Statements:\n");
    for (auto I : toBeErased) {
      DEBUG(errs() << *I << "\n");
    }
    while (!toBeErased.empty()) {
      Instruction *I = toBeErased.back();
      DEBUG(errs() << "\tErasing " << *I << "\n");
      I->eraseFromParent();
      toBeErased.pop_back();
    }

    if (bind == mutateTypeCause::mtc_BIND ||
        bind == mutateTypeCause::mtc_RETURN) {
      DEBUG(errs() << "Function before fixing return type\n" << *f << "\n");
      // Argument type list.
      std::vector<Type *> FArgTypes;
      for (Function::const_arg_iterator ai = f->arg_begin(), ae = f->arg_end();
           ai != ae; ++ai) {
        FArgTypes.push_back(ai->getType());
      }

      // Find new return type of function
      Type *NewReturnTy;
      if (bind == mutateTypeCause::mtc_BIND) {

        std::vector<Type *> TyList;
        for (unsigned i = 0; i < FRetTypes.size(); i++)
          TyList.push_back(FRetTypes[i]);

        NewReturnTy =
            StructType::create(f->getContext(), TyList,
                               Twine("struct.out." + f->getName()).str(), true);
      } else {
        NewReturnTy = getReturnTypeFromReturnInst(f);
        assert(NewReturnTy->isStructTy() && "Expecting a struct type!");
      }

      FunctionType *FTy =
          FunctionType::get(NewReturnTy, FArgTypes, f->isVarArg());

      // Change the function type
      Function *newF = cloneFunction(f, FTy, false);
      DEBUG(errs() << *newF << "\n");

      if (bind == mutateTypeCause::mtc_BIND) {
        // This is certainly an internal node, and hence just one BB with one
        // return terminator instruction. Change return statement
        ReturnInst *RI =
            cast<ReturnInst>(newF->getEntryBlock().getTerminator());
        ReturnInst *newRI = ReturnInst::Create(newF->getContext(),
                                               UndefValue::get(NewReturnTy));
        ReplaceInstWithInst(RI, newRI);
      }
      if (bind == mutateTypeCause::mtc_RETURN) {
        // Nothing
      }
      replaceNodeFunctionInIR(*f->getParent(), f, newF);
      DEBUG(errs() << "Function after fixing return type\n" << *newF << "\n");
    }
  }
  return false; // TODO: What does returning "false" mean?
}

// Generate Code for declaring a constant string [L x i8] and return a pointer
// to the start of it.
Value *GenHPVM::getStringPointer(const Twine &S, Instruction *IB,
                                 const Twine &Name) {
  Constant *SConstant =
      ConstantDataArray::getString(M->getContext(), S.str(), true);
  Value *SGlobal =
      new GlobalVariable(*M, SConstant->getType(), true,
                         GlobalValue::InternalLinkage, SConstant, Name);
  Value *Zero = ConstantInt::get(Type::getInt64Ty(M->getContext()), 0);
  Value *GEPArgs[] = {Zero, Zero};
  GetElementPtrInst *SPtr = GetElementPtrInst::Create(
      nullptr, SGlobal, ArrayRef<Value *>(GEPArgs, 2), Name + "Ptr", IB);
  return SPtr;
}

void GenHPVM::initializeTimerSet(Instruction *InsertBefore) {
  Value *TimerSetAddr;
  StoreInst *SI;
  TIMER(TimerSet = new GlobalVariable(
            *M, Type::getInt8PtrTy(M->getContext()), false,
            GlobalValue::CommonLinkage,
            Constant::getNullValue(Type::getInt8PtrTy(M->getContext())),
            "hpvmTimerSet_GenHPVM"));
  DEBUG(errs() << "Inserting GV: " << *TimerSet->getType() << *TimerSet
               << "\n");
  // DEBUG(errs() << "Inserting call to: " << *llvm_hpvm_initializeTimerSet <<
  // "\n");

  TIMER(TimerSetAddr = CallInst::Create(llvm_hpvm_initializeTimerSet, None, "",
                                        InsertBefore));
  DEBUG(errs() << "TimerSetAddress = " << *TimerSetAddr << "\n");
  TIMER(SI = new StoreInst(TimerSetAddr, TimerSet, InsertBefore));
  DEBUG(errs() << "Store Timer Address in Global variable: " << *SI << "\n");
}

void GenHPVM::switchToTimer(enum hpvm_TimerID timer,
                            Instruction *InsertBefore) {
  Value *switchArgs[] = {TimerSet, getTimerID(*M, timer)};
  TIMER(CallInst::Create(llvm_hpvm_switchToTimer,
                         ArrayRef<Value *>(switchArgs, 2), "", InsertBefore));
}

void GenHPVM::printTimerSet(Instruction *InsertBefore) {
  Value *TimerName;
  TIMER(TimerName = getStringPointer("GenHPVM_Timer", InsertBefore));
  Value *printArgs[] = {TimerSet, TimerName};
  TIMER(CallInst::Create(llvm_hpvm_printTimerSet,
                         ArrayRef<Value *>(printArgs, 2), "", InsertBefore));
}

static inline ConstantInt *getTimerID(Module &M, enum hpvm_TimerID timer) {
  return ConstantInt::get(Type::getInt32Ty(M.getContext()), timer);
}

static Function *transformReturnTypeToStruct(Function *F) {
  // Currently only works for void return types
  DEBUG(errs() << "Transforming return type of function to Struct: "
               << F->getName() << "\n");

  if (isa<StructType>(F->getReturnType())) {
    DEBUG(errs() << "Return type is already a Struct: " << F->getName() << ": "
                 << *F->getReturnType() << "\n");
    return F;
  }

  assert(F->getReturnType()->isVoidTy() &&
         "Unhandled case - Only void return type handled\n");

  // Create the argument type list with added argument types
  std::vector<Type *> ArgTypes;
  for (Function::const_arg_iterator ai = F->arg_begin(), ae = F->arg_end();
       ai != ae; ++ai) {
    ArgTypes.push_back(ai->getType());
  }

  StructType *RetTy =
      StructType::create(F->getContext(), None, "emptyStruct", true);
  FunctionType *FTy = FunctionType::get(RetTy, ArgTypes, F->isVarArg());

  SmallVector<ReturnInst *, 8> Returns;
  Function *newF = cloneFunction(F, FTy, false, &Returns);
  // Replace ret void instruction with ret %RetTy undef
  for (auto &RI : Returns) {
    DEBUG(errs() << "Found return inst: " << *RI << "\n");
    ReturnInst *newRI =
        ReturnInst::Create(newF->getContext(), UndefValue::get(RetTy));
    ReplaceInstWithInst(RI, newRI);
  }

  replaceNodeFunctionInIR(*F->getParent(), F, newF);
  return newF;
}

static Type *getReturnTypeFromReturnInst(Function *F) {
  for (BasicBlock &BB : *F) {
    if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      DEBUG(errs() << "Return type value: " << *RI->getReturnValue()->getType()
                   << "\n");
      return RI->getReturnValue()->getType();
    }
  }
  return NULL;
}

char genhpvm::GenHPVM::ID = 0;
static RegisterPass<genhpvm::GenHPVM>
    X("genhpvm",
      "Pass to generate HPVM IR from LLVM IR (with dummy function calls)",
      false, false);

} // End of namespace genhpvm
