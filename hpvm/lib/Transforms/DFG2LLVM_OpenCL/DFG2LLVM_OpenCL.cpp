//===----------------------- DFG2LLVM_OpenCL.cpp ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// 
// This pass is responsible for generating code for kernel code and code for 
// launching kernels for GPU target using HPVM dataflow graph. The kernels are
// generated into a separate file which is the C-Backend uses to generate 
// OpenCL kernels with.
//
//===----------------------------------------------------------------------===//


#define ENABLE_ASSERTS
#define TARGET_PTX 64
#define GENERIC_ADDRSPACE 0
#define GLOBAL_ADDRSPACE 1
#define CONSTANT_ADDRSPACE 4
#define SHARED_ADDRSPACE 3

#define DEBUG_TYPE "DFG2LLVM_OpenCL"
#include "SupportHPVM/DFG2LLVM.h"
#include "SupportHPVM/HPVMTimer.h"
#include "SupportHPVM/HPVMUtils.h"
#include "llvm-c/Core.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/UseListOrder.h"
#include "llvm/Support/ToolOutputFile.h"

#include <sstream>

#ifndef LLVM_BUILD_DIR
#error LLVM_BUILD_DIR is not defined
#endif

#define STR_VALUE(X) #X
#define STRINGIFY(X) STR_VALUE(X)
#define LLVM_BUILD_DIR_STR STRINGIFY(LLVM_BUILD_DIR)

using namespace llvm;
using namespace builddfg;
using namespace dfg2llvm;
using namespace hpvmUtils;

// HPVM Command line option to use timer or not
static cl::opt<bool> HPVMTimer_OpenCL("hpvm-timers-ptx",
                                      cl::desc("Enable hpvm timers"));

namespace {
// Helper class declarations

// Class to maintain the tuple of host pointer, device pointer and size
// in bytes. Would have preferred to use tuple but support not yet available
class OutputPtr {
public:
  OutputPtr(Value *_h_ptr, Value *_d_ptr, Value *_bytes)
      : h_ptr(_h_ptr), d_ptr(_d_ptr), bytes(_bytes) {}

  Value *h_ptr;
  Value *d_ptr;
  Value *bytes;
};

// Class to maintain important kernel info required for generating runtime
// calls
class Kernel {
public:
  Kernel(
      Function *_KF, DFLeafNode *_KLeafNode,
      std::map<unsigned, unsigned> _inArgMap = std::map<unsigned, unsigned>(),
      std::map<unsigned, std::pair<Value *, unsigned>> _sharedInArgMap =
          std::map<unsigned, std::pair<Value *, unsigned>>(),
      std::vector<unsigned> _outArgMap = std::vector<unsigned>(),
      unsigned _gridDim = 0,
      std::vector<Value *> _globalWGSize = std::vector<Value *>(),
      unsigned _blockDim = 0,
      std::vector<Value *> _localWGSize = std::vector<Value *>())
      : KernelFunction(_KF), KernelLeafNode(_KLeafNode), inArgMap(_inArgMap),
        sharedInArgMap(_sharedInArgMap), outArgMap(_outArgMap),
        gridDim(_gridDim), globalWGSize(_globalWGSize), blockDim(_blockDim),
        localWGSize(_localWGSize) {

    assert(gridDim == globalWGSize.size() &&
           "gridDim should be same as the size of vector globalWGSize");
    assert(blockDim == localWGSize.size() &&
           "blockDim should be same as the size of vector localWGSize");
  }

  Function *KernelFunction;
  DFLeafNode *KernelLeafNode;
  std::map<unsigned, unsigned> inArgMap;
  // Map for shared memory arguments
  std::map<unsigned, std::pair<Value *, unsigned>> sharedInArgMap;
  // Fields for (potential) allocation node
  DFLeafNode *AllocationNode;
  Function *AllocationFunction;
  std::map<unsigned, unsigned> allocInArgMap;

  std::vector<unsigned> outArgMap;
  unsigned gridDim;
  std::vector<Value *> globalWGSize;
  unsigned blockDim;
  std::vector<Value *> localWGSize;
  std::vector<int> localDimMap;

  std::map<unsigned, unsigned> &getInArgMap() { return inArgMap; }
  void setInArgMap(std::map<unsigned, unsigned> map) { inArgMap = map; }

  std::map<unsigned, std::pair<Value *, unsigned>> &getSharedInArgMap() {
    return sharedInArgMap;
  }
  void setSharedInArgMap(std::map<unsigned, std::pair<Value *, unsigned>> map) {
    sharedInArgMap = map;
  }

  std::vector<unsigned> &getOutArgMap() { return outArgMap; }
  void setOutArgMap(std::vector<unsigned> map) { outArgMap = map; }

  void setLocalWGSize(std::vector<Value *> V) { localWGSize = V; }

  bool hasLocalWG() const { return blockDim != 0; }
};

// Helper function declarations
static bool canBePromoted(Argument *arg, Function *F);
static void getExecuteNodeParams(Module &M, Value *&, Value *&, Value *&,
                                 Kernel *, ValueToValueMapTy &, Instruction *);
static Value *genWorkGroupPtr(Module &M, std::vector<Value *>,
                              ValueToValueMapTy &, Instruction *,
                              const Twine &WGName = "WGSize");
static std::string getPTXFilename(const Module &);
static void changeDataLayout(Module &);
static void changeTargetTriple(Module &);
static void findReturnInst(Function *, std::vector<ReturnInst *> &);
static void findIntrinsicInst(Function *, Intrinsic::ID,
                              std::vector<IntrinsicInst *> &);

// DFG2LLVM_OpenCL - The first implementation.
struct DFG2LLVM_OpenCL : public DFG2LLVM {
  static char ID; // Pass identification, replacement for typeid
  DFG2LLVM_OpenCL() : DFG2LLVM(ID) {}

private:
public:
  bool runOnModule(Module &M);
};

// Visitor for Code generation traversal (tree traversal for now)
class CGT_OpenCL : public CodeGenTraversal {

private:
  // Member variables
  std::unique_ptr<Module> KernelM;
  DFNode *KernelLaunchNode = NULL;
  Kernel *kernel;

  // HPVM Runtime API
  FunctionCallee llvm_hpvm_ocl_launch;
  FunctionCallee llvm_hpvm_ocl_wait;
  FunctionCallee llvm_hpvm_ocl_initContext;
  FunctionCallee llvm_hpvm_ocl_clearContext;
  FunctionCallee llvm_hpvm_ocl_argument_shared;
  FunctionCallee llvm_hpvm_ocl_argument_scalar;
  FunctionCallee llvm_hpvm_ocl_argument_ptr;
  FunctionCallee llvm_hpvm_ocl_output_ptr;
  FunctionCallee llvm_hpvm_ocl_free;
  FunctionCallee llvm_hpvm_ocl_getOutput;
  FunctionCallee llvm_hpvm_ocl_executeNode;

  // Functions
  std::string getKernelsModuleName(Module &M);
  void fixValueAddrspace(Value *V, unsigned addrspace);
  std::vector<unsigned> globalToConstantMemoryOpt(std::vector<unsigned> *,
                                                  Function *);
  Function *changeArgAddrspace(Function *F, std::vector<unsigned> &Ags,
                               unsigned i);
  void addCLMetadata(Function *F);
  Function *transformFunctionToVoid(Function *F);
  void insertRuntimeCalls(DFInternalNode *N, Kernel *K, const Twine &FileName);

  // Virtual Functions
  void init() {
    HPVMTimer = HPVMTimer_OpenCL;
    TargetName = "OpenCL";
  }
  void initRuntimeAPI();
  void codeGen(DFInternalNode *N);
  void codeGen(DFLeafNode *N);

public:
  // Constructor
  CGT_OpenCL(Module &_M, BuildDFG &_DFG)
      : CodeGenTraversal(_M, _DFG), KernelM(CloneModule(_M)) {
    init();
    initRuntimeAPI();
    DEBUG(errs() << "Old module pointer: " << &_M << "\n");
    DEBUG(errs() << "New module pointer: " << KernelM.get() << "\n");

    // Copying instead of creating new, in order to preserve required info
    // (metadata) Remove functions, global variables and aliases
    std::vector<GlobalVariable *> GVVect;
    for (Module::global_iterator mi = KernelM->global_begin(),
                                 me = KernelM->global_end();
         (mi != me); ++mi) {
      GlobalVariable *GV = &*mi;
      GVVect.push_back(GV);
    }
    for (auto *GV : GVVect) {
      GV->replaceAllUsesWith(UndefValue::get(GV->getType()));
      GV->eraseFromParent();
    }

    std::vector<Function *> FuncVect;
    for (Module::iterator mi = KernelM->begin(), me = KernelM->end();
         (mi != me); ++mi) {
      Function *F = &*mi;
      FuncVect.push_back(F);
    }
    for (auto *F : FuncVect) {
      F->replaceAllUsesWith(UndefValue::get(F->getType()));
      F->eraseFromParent();
    }

    std::vector<GlobalAlias *> GAVect;
    for (Module::alias_iterator mi = KernelM->alias_begin(),
                                me = KernelM->alias_end();
         (mi != me); ++mi) {
      GlobalAlias *GA = &*mi;
      GAVect.push_back(GA);
    }
    for (auto *GA : GAVect) {
      GA->replaceAllUsesWith(UndefValue::get(GA->getType()));
      GA->eraseFromParent();
    }

    changeDataLayout(*KernelM);
    changeTargetTriple(*KernelM);

    DEBUG(errs() << *KernelM);
  }

  void writeKernelsModule();
};

// Initialize the HPVM runtime API. This makes it easier to insert these calls
void CGT_OpenCL::initRuntimeAPI() {

  // Load Runtime API Module
  SMDiagnostic Err;

  std::string runtimeAPI = std::string(LLVM_BUILD_DIR_STR) +
                           "/tools/hpvm/projects/hpvm-rt/hpvm-rt.bc";

  runtimeModule = parseIRFile(runtimeAPI, Err, M.getContext());
  if (runtimeModule == nullptr) {
    DEBUG(errs() << Err.getMessage() << " " << runtimeAPI << "\n");
    assert(false && "couldn't parse runtime");
  } else
    DEBUG(errs() << "Successfully loaded hpvm-rt API module\n");

  // Get or insert the global declarations for launch/wait functions
  DECLARE(llvm_hpvm_ocl_launch);
  DECLARE(llvm_hpvm_ocl_wait);
  DECLARE(llvm_hpvm_ocl_initContext);
  DECLARE(llvm_hpvm_ocl_clearContext);
  DECLARE(llvm_hpvm_ocl_argument_shared);
  DECLARE(llvm_hpvm_ocl_argument_scalar);
  DECLARE(llvm_hpvm_ocl_argument_ptr);
  DECLARE(llvm_hpvm_ocl_output_ptr);
  DECLARE(llvm_hpvm_ocl_free);
  DECLARE(llvm_hpvm_ocl_getOutput);
  DECLARE(llvm_hpvm_ocl_executeNode);

  // Get or insert timerAPI functions as well if you plan to use timers
  initTimerAPI();

  // Insert init context in main
  DEBUG(errs() << "Gen Code to initialize OpenCL Timer\n");
  Function *VI = M.getFunction("llvm.hpvm.init");
  assert(VI->getNumUses() == 1 && "__hpvm__init should only be used once");

  InitCall = cast<Instruction>(*VI->user_begin());
  initializeTimerSet(InitCall);
  switchToTimer(hpvm_TimerID_INIT_CTX, InitCall);
  CallInst::Create(llvm_hpvm_ocl_initContext,
                   ArrayRef<Value *>(getTargetID(M, hpvm::GPU_TARGET)), "",
                   InitCall);
  switchToTimer(hpvm_TimerID_NONE, InitCall);

  // Insert print instruction at hpvm exit
  DEBUG(errs() << "Gen Code to print OpenCL Timer\n");
  Function *VC = M.getFunction("llvm.hpvm.cleanup");
  DEBUG(errs() << *VC << "\n");
  assert(VC->getNumUses() == 1 && "__hpvm__clear should only be used once");

  CleanupCall = cast<Instruction>(*VC->user_begin());
  printTimerSet(CleanupCall);
}

// Generate Code to call the kernel
// The plan is to replace the internal node with a leaf node. This method is
// used to generate a function to associate with this leaf node. The function
// is responsible for all the memory allocation/transfer and invoking the
// kernel call on the device
void CGT_OpenCL::insertRuntimeCalls(DFInternalNode *N, Kernel *K,
                                    const Twine &FileName) {
  // Check if clone already exists. If it does, it means we have visited this
  // function before.
  //  assert(N->getGenFunc() == NULL && "Code already generated for this node");

  assert(N->getGenFuncForTarget(hpvm::GPU_TARGET) == NULL &&
         "Code already generated for this node");

  // Useful values
  Value *True = ConstantInt::get(Type::getInt1Ty(M.getContext()), 1);
  Value *False = ConstantInt::get(Type::getInt1Ty(M.getContext()), 0);

  // If kernel struct has not been initialized with kernel function, then fail
  assert(K != NULL && "No kernel found!!");

  DEBUG(errs() << "Generating kernel call code\n");

  Function *F = N->getFuncPointer();

  // Create of clone of F with no instructions. Only the type is the same as F
  // without the extra arguments.
  Function *F_CPU;

  // Clone the function, if we are seeing this function for the first time. We
  // only need a clone in terms of type.
  ValueToValueMapTy VMap;

  // Create new function with the same type
  F_CPU =
      Function::Create(F->getFunctionType(), F->getLinkage(), F->getName(), &M);

  // Loop over the arguments, copying the names of arguments over.
  Function::arg_iterator dest_iterator = F_CPU->arg_begin();
  for (Function::const_arg_iterator i = F->arg_begin(), e = F->arg_end();
       i != e; ++i) {
    dest_iterator->setName(i->getName()); // Copy the name over...
    // Increment dest iterator
    ++dest_iterator;
  }

  // Add a basic block to this empty function
  BasicBlock *BB = BasicBlock::Create(M.getContext(), "entry", F_CPU);
  ReturnInst *RI = ReturnInst::Create(
      M.getContext(), UndefValue::get(F_CPU->getReturnType()), BB);

  // FIXME: Adding Index and Dim arguments are probably not required except
  // for consistency purpose (DFG2LLVM_CPU does assume that all leaf nodes do
  // have those arguments)

  // Add Index and Dim arguments except for the root node
  if (!N->isRoot() && !N->getParent()->isChildGraphStreaming())
    F_CPU = addIdxDimArgs(F_CPU);

  BB = &*F_CPU->begin();
  RI = cast<ReturnInst>(BB->getTerminator());

  // Add the generated function info to DFNode
  //  N->setGenFunc(F_CPU, hpvm::CPU_TARGET);
  N->addGenFunc(F_CPU, hpvm::GPU_TARGET, true);
  DEBUG(errs() << "Added GPUGenFunc: " << F_CPU->getName() << " for node "
               << N->getFuncPointer()->getName() << "\n");

  // Loop over the arguments, to create the VMap
  dest_iterator = F_CPU->arg_begin();
  for (Function::const_arg_iterator i = F->arg_begin(), e = F->arg_end();
       i != e; ++i) {
    // Add mapping to VMap and increment dest iterator
    VMap[&*i] = &*dest_iterator;
    ++dest_iterator;
  }

  /* TODO: Use this code to verufy if this is a good pattern for PTX kernel

  // Sort children in topological order before code generation for kernel call
  N->getChildGraph()->sortChildren();

  // The DFNode N has the property that it has only one child (leaving Entry
  // and Exit dummy nodes). This child is the PTX kernel. This simplifies code
  // generation for kernel calls significantly. All the inputs to this child
  // node would either be constants or from the parent node N.

  assert(N->getChildGraph()->size() == 3
         && "Node expected to have just one non-dummy node!");

  DFNode* C;
  for(DFGraph::children_iterator ci = N->getChildGraph()->begin(),
      ce = N->getChildGraph()->end(); ci != ce; ++ci) {
    C = *ci;
    // Skip dummy node call
    if (!C->isDummyNode())
      break;
  }

  assert(C->isDummyNode() == false && "Internal Node only contains dummy
  nodes!");

  Function* CF = C->getFuncPointer();
  */
  Function *KF = K->KernelLeafNode->getFuncPointer();
  // Initialize context
  // DEBUG(errs() << "Initializing context" << "\n");
  // CallInst::Create(llvm_hpvm_ocl_initContext, None, "", RI);

  DEBUG(errs() << "Initializing commandQ"
               << "\n");
  // Initialize command queue
  switchToTimer(hpvm_TimerID_SETUP, InitCall);
  Value *fileStr = getStringPointer(FileName, InitCall, "Filename");
  DEBUG(errs() << "Kernel Filename constant: " << *fileStr << "\n");
  DEBUG(errs() << "Generating code for kernel - "
               << K->KernelFunction->getName() << "\n");
  Value *kernelStr =
      getStringPointer(K->KernelFunction->getName(), InitCall, "KernelName");

  Value *LaunchInstArgs[] = {fileStr, kernelStr};

  DEBUG(errs() << "Inserting launch call"
               << "\n");
  CallInst *OpenCL_Ctx = CallInst::Create(llvm_hpvm_ocl_launch,
                                          ArrayRef<Value *>(LaunchInstArgs, 2),
                                          "graph" + KF->getName(), InitCall);
  DEBUG(errs() << *OpenCL_Ctx << "\n");
  GraphIDAddr = new GlobalVariable(
      M, OpenCL_Ctx->getType(), false, GlobalValue::CommonLinkage,
      Constant::getNullValue(OpenCL_Ctx->getType()),
      "graph" + KF->getName() + ".addr");
  DEBUG(errs() << "Store at: " << *GraphIDAddr << "\n");
  StoreInst *SI = new StoreInst(OpenCL_Ctx, GraphIDAddr, InitCall);
  DEBUG(errs() << *SI << "\n");
  switchToTimer(hpvm_TimerID_NONE, InitCall);
  switchToTimer(hpvm_TimerID_SETUP, RI);
  Value *GraphID = new LoadInst(GraphIDAddr, "graph." + KF->getName(), RI);

  // Iterate over the required input edges of the node and use the hpvm-rt API
  // to set inputs
  DEBUG(errs() << "Iterate over input edges of node and insert hpvm api\n");
  std::vector<OutputPtr> OutputPointers;
  // Vector to hold the device memory object that need to be cleared before we
  // release context
  std::vector<Value *> DevicePointers;

  std::map<unsigned, unsigned> &kernelInArgMap = K->getInArgMap();
  /*
    for(unsigned i=0; i<KF->getFunctionType()->getNumParams(); i++) {

      // The kernel object gives us the mapping of arguments from kernel launch
      // node function (F_CPU) to kernel (kernel->KF)
      Value* inputVal = getArgumentAt(F_CPU, K->getInArgMap()[i]);

  */

  for (auto &InArgMapPair : kernelInArgMap) {
    unsigned i = InArgMapPair.first;
    Value *inputVal = getArgumentAt(F_CPU, InArgMapPair.second);
    DEBUG(errs() << "\tArgument " << i << " = " << *inputVal << "\n");

    // input value has been obtained.
    // Check if input is a scalar value or a pointer operand
    // For scalar values such as int, float, etc. the size is simply the size of
    // type on target machine, but for pointers, the size of data would be the
    // next integer argument
    if (inputVal->getType()->isPointerTy()) {

      switchToTimer(hpvm_TimerID_COPY_PTR, RI);
      // Pointer Input
      // CheckAttribute
      Value *isOutput = (hasAttribute(KF, i, Attribute::Out)) ? True : False;
      Value *isInput = ((hasAttribute(KF, i, Attribute::Out)) &&
                        !(hasAttribute(KF, i, Attribute::In)))
                           ? False
                           : True;

      Argument *A = getArgumentAt(KF, i);
      if (isOutput == True) {
        DEBUG(errs() << *A << " is an OUTPUT argument\n");
      }
      if (isInput == True) {
        DEBUG(errs() << *A << " is an INPUT argument\n");
      }

      Value *inputValI8Ptr = CastInst::CreatePointerCast(
          inputVal, Type::getInt8PtrTy(M.getContext()),
          inputVal->getName() + ".i8ptr", RI);

      // Assert that the pointer argument size (next argument) is in the map
      assert(kernelInArgMap.find(i + 1) != kernelInArgMap.end());

      Value *inputSize = getArgumentAt(F_CPU, kernelInArgMap[i + 1]);
      assert(
          inputSize->getType() == Type::getInt64Ty(M.getContext()) &&
          "Pointer type input must always be followed by size (integer type)");
      Value *setInputArgs[] = {
          GraphID,
          inputValI8Ptr,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), i),
          inputSize,
          isInput,
          isOutput};
      Value *d_ptr =
          CallInst::Create(llvm_hpvm_ocl_argument_ptr,
                           ArrayRef<Value *>(setInputArgs, 6), "", RI);
      DevicePointers.push_back(d_ptr);
      // If this has out attribute, store the returned device pointer in
      // memory to read device memory later
      if (isOutput == True)
        OutputPointers.push_back(OutputPtr(inputValI8Ptr, d_ptr, inputSize));
    } else {
      switchToTimer(hpvm_TimerID_COPY_SCALAR, RI);
      // Scalar Input
      // Store the scalar value on stack and then pass the pointer to its
      // location
      AllocaInst *inputValPtr = new AllocaInst(
          inputVal->getType(), 0, inputVal->getName() + ".ptr", RI);
      new StoreInst(inputVal, inputValPtr, RI);

      Value *inputValI8Ptr = CastInst::CreatePointerCast(
          inputValPtr, Type::getInt8PtrTy(M.getContext()),
          inputVal->getName() + ".i8ptr", RI);

      Value *setInputArgs[] = {
          GraphID, inputValI8Ptr,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), i),
          ConstantExpr::getSizeOf(inputVal->getType())};
      CallInst::Create(llvm_hpvm_ocl_argument_scalar,
                       ArrayRef<Value *>(setInputArgs, 4), "", RI);
    }
  }

  DEBUG(
      errs() << "Setup shared memory arguments of node and insert hpvm api\n");

  // Check to see if all the allocation sizes are constant (determined
  // statically)
  bool constSizes = true;
  for (auto &e : K->getSharedInArgMap()) {
    constSizes &= isa<Constant>(e.second.first);
  }

  // If the sizes are all constant
  if (constSizes) {
    for (auto &e : K->getSharedInArgMap()) {
      unsigned argNum = e.first;
      Value *allocSize = e.second.first;

      DEBUG(errs() << "\tLocal Memory at " << argNum
                   << ", size = " << *allocSize << "\n");

      if (KF->getFunctionType()->getParamType(argNum)->isPointerTy()) {
        // Shared memory ptr argument - scalar at size position
        switchToTimer(hpvm_TimerID_COPY_SCALAR, RI);

        assert(isa<Constant>(allocSize) &&
               "Constant shared memory size is expected");

        Value *setInputArgs[] = {
            GraphID, ConstantInt::get(Type::getInt32Ty(M.getContext()), argNum),
            allocSize};
        CallInst::Create(llvm_hpvm_ocl_argument_shared,
                         ArrayRef<Value *>(setInputArgs, 3), "", RI);
      } else {
        // Sharem memory size argument - scalar at address position
        switchToTimer(hpvm_TimerID_COPY_SCALAR, RI);
        // Store the scalar value on stack and then pass the pointer to its
        // location
        AllocaInst *allocSizePtr =
            new AllocaInst(allocSize->getType(), 0,
                           allocSize->getName() + ".sharedMem.ptr", RI);
        new StoreInst(allocSize, allocSizePtr, RI);

        Value *allocSizeI8Ptr = CastInst::CreatePointerCast(
            allocSizePtr, Type::getInt8PtrTy(M.getContext()),
            allocSize->getName() + ".sharedMem.i8ptr", RI);

        Value *setInputArgs[] = {
            GraphID, allocSizeI8Ptr,
            ConstantInt::get(Type::getInt32Ty(M.getContext()), argNum),
            ConstantExpr::getSizeOf(allocSize->getType())};
        CallInst::Create(llvm_hpvm_ocl_argument_scalar,
                         ArrayRef<Value *>(setInputArgs, 4), "", RI);
      }
    }
  } else {

    Function *F_alloc = K->AllocationFunction;
    StructType *FAllocRetTy = dyn_cast<StructType>(F_alloc->getReturnType());
    assert(FAllocRetTy && "Allocation node with no struct return type");

    std::vector<Value *> AllocInputArgs;
    for (unsigned i = 0; i < K->allocInArgMap.size(); i++) {
      AllocInputArgs.push_back(getArgumentAt(F_CPU, K->allocInArgMap.at(i)));
    }

    CallInst *CI = CallInst::Create(F_alloc, AllocInputArgs, "", RI);
    std::vector<ExtractValueInst *> ExtractValueInstVec;
    for (unsigned i = 1; i < FAllocRetTy->getNumElements(); i += 2) {
      ExtractValueInst *EI = ExtractValueInst::Create(CI, i, "", RI);
      ExtractValueInstVec.push_back(EI);
    }

    for (auto &e : K->getSharedInArgMap()) {
      unsigned argNum = e.first;
      Value *allocSize = ExtractValueInstVec[e.second.second / 2];

      DEBUG(errs() << "\tLocal Memory at " << argNum
                   << ", size = " << *allocSize << "\n");

      if (KF->getFunctionType()->getParamType(argNum)->isPointerTy()) {
        // Shared memory ptr argument - scalar at size position
        switchToTimer(hpvm_TimerID_COPY_SCALAR, RI);

        Value *setInputArgs[] = {
            GraphID, ConstantInt::get(Type::getInt32Ty(M.getContext()), argNum),
            allocSize};
        CallInst::Create(llvm_hpvm_ocl_argument_shared,
                         ArrayRef<Value *>(setInputArgs, 3), "", RI);
      } else {
        // Sharem memory size argument - scalar at address position
        switchToTimer(hpvm_TimerID_COPY_SCALAR, RI);
        // Store the scalar value on stack and then pass the pointer to its
        // location
        AllocaInst *allocSizePtr =
            new AllocaInst(allocSize->getType(), 0,
                           allocSize->getName() + ".sharedMem.ptr", RI);
        new StoreInst(allocSize, allocSizePtr, RI);

        Value *allocSizeI8Ptr = CastInst::CreatePointerCast(
            allocSizePtr, Type::getInt8PtrTy(M.getContext()),
            allocSize->getName() + ".sharedMem.i8ptr", RI);

        Value *setInputArgs[] = {
            GraphID, allocSizeI8Ptr,
            ConstantInt::get(Type::getInt32Ty(M.getContext()), argNum),
            ConstantExpr::getSizeOf(allocSize->getType())};
        CallInst::Create(llvm_hpvm_ocl_argument_scalar,
                         ArrayRef<Value *>(setInputArgs, 4), "", RI);
      }
    }
  }

  DEBUG(errs() << "Setup output edges of node and insert hpvm api\n");
  // Set output if struct is not an empty struct
  StructType *OutputTy = K->KernelLeafNode->getOutputType();
  std::vector<Value *> d_Outputs;
  if (!OutputTy->isEmptyTy()) {
    switchToTimer(hpvm_TimerID_COPY_PTR, RI);
    // Not an empty struct
    // Iterate over all elements of the struct and put them in
    for (unsigned i = 0; i < OutputTy->getNumElements(); i++) {
      unsigned outputIndex = KF->getFunctionType()->getNumParams() + i;
      Value *setOutputArgs[] = {
          GraphID,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), outputIndex),
          ConstantExpr::getSizeOf(OutputTy->getElementType(i))};

      CallInst *d_Output = CallInst::Create(llvm_hpvm_ocl_output_ptr,
                                            ArrayRef<Value *>(setOutputArgs, 3),
                                            "d_output." + KF->getName(), RI);
      d_Outputs.push_back(d_Output);
    }
  }

  // Enqueue kernel
  // Need work dim, localworksize, globalworksize
  // Allocate size_t[numDims] space on stack. Store the work group sizes and
  // pass it as an argument to ExecNode

  switchToTimer(hpvm_TimerID_MISC, RI);
  Value *workDim, *LocalWGPtr, *GlobalWGPtr;
  getExecuteNodeParams(M, workDim, LocalWGPtr, GlobalWGPtr, K, VMap, RI);
  switchToTimer(hpvm_TimerID_KERNEL, RI);
  Value *ExecNodeArgs[] = {GraphID, workDim, LocalWGPtr, GlobalWGPtr};
  CallInst *Event = CallInst::Create(llvm_hpvm_ocl_executeNode,
                                     ArrayRef<Value *>(ExecNodeArgs, 4),
                                     "event." + KF->getName(), RI);
  DEBUG(errs() << "Execute Node Call: " << *Event << "\n");

  // Wait for Kernel to Finish
  CallInst::Create(llvm_hpvm_ocl_wait, ArrayRef<Value *>(GraphID), "", RI);

  switchToTimer(hpvm_TimerID_READ_OUTPUT, RI);
  // Read Output Struct if not empty
  if (!OutputTy->isEmptyTy()) {
    std::vector<Value *> h_Outputs;
    Value *KernelOutput = UndefValue::get(OutputTy);
    for (unsigned i = 0; i < OutputTy->getNumElements(); i++) {
      Value *GetOutputArgs[] = {
          GraphID, Constant::getNullValue(Type::getInt8PtrTy(M.getContext())),
          d_Outputs[i], ConstantExpr::getSizeOf(OutputTy->getElementType(i))};
      CallInst *h_Output = CallInst::Create(
          llvm_hpvm_ocl_getOutput, ArrayRef<Value *>(GetOutputArgs, 4),
          "h_output." + KF->getName() + ".addr", RI);
      // Read each device pointer listed in output struct
      // Load the output struct
      CastInst *BI = BitCastInst::CreatePointerCast(
          h_Output, OutputTy->getElementType(i)->getPointerTo(), "output.ptr",
          RI);

      Value *OutputElement = new LoadInst(BI, "output." + KF->getName(), RI);
      KernelOutput = InsertValueInst::Create(KernelOutput, OutputElement,
                                             ArrayRef<unsigned>(i),
                                             KF->getName() + "output", RI);
    }
    OutputMap[K->KernelLeafNode] = KernelOutput;
  }

  // Read all the pointer arguments which had side effects i.e., had out
  // attribute
  DEBUG(errs() << "Output Pointers : " << OutputPointers.size() << "\n");
  // FIXME: Not reading output pointers anymore as we read them when data is
  // actually requested
  /*for(auto output: OutputPointers) {
    DEBUG(errs() << "Read: " << *output.d_ptr << "\n");
    DEBUG(errs() << "\tTo: " << *output.h_ptr << "\n");
    DEBUG(errs() << "\t#bytes: " << *output.bytes << "\n");

    Value* GetOutputArgs[] = {GraphID, output.h_ptr, output.d_ptr,
  output.bytes}; CallInst* CI = CallInst::Create(llvm_hpvm_ocl_getOutput,
                                    ArrayRef<Value*>(GetOutputArgs, 4),
                                    "", RI);
  }*/
  switchToTimer(hpvm_TimerID_MEM_FREE, RI);
  // Clear Context and free device memory
  DEBUG(errs() << "Clearing context"
               << "\n");
  // Free Device Memory
  for (auto d_ptr : DevicePointers) {
    CallInst::Create(llvm_hpvm_ocl_free, ArrayRef<Value *>(d_ptr), "", RI);
  }
  switchToTimer(hpvm_TimerID_CLEAR_CTX, CleanupCall);
  // Clear Context
  LoadInst *LI = new LoadInst(GraphIDAddr, "", CleanupCall);
  CallInst::Create(llvm_hpvm_ocl_clearContext, ArrayRef<Value *>(LI), "",
                   CleanupCall);
  switchToTimer(hpvm_TimerID_NONE, CleanupCall);

  switchToTimer(hpvm_TimerID_MISC, RI);
  DEBUG(errs() << "*** Generating epilogue code for the function****\n");
  // Generate code for output bindings
  // Get Exit node
  DFNode *C = N->getChildGraph()->getExit();
  // Get OutputType of this node
  StructType *OutTy = N->getOutputType();
  Value *retVal = UndefValue::get(F_CPU->getReturnType());
  // Find the kernel's output arg map, to use instead of the bindings
  std::vector<unsigned> outArgMap = kernel->getOutArgMap();
  // Find all the input edges to exit node
  for (unsigned i = 0; i < OutTy->getNumElements(); i++) {
    DEBUG(errs() << "Output Edge " << i << "\n");
    // Find the incoming edge at the requested input port
    DFEdge *E = C->getInDFEdgeAt(i);

    assert(E && "No Binding for output element!");
    // Find the Source DFNode associated with the incoming edge
    DFNode *SrcDF = E->getSourceDF();

    DEBUG(errs() << "Edge source -- " << SrcDF->getFuncPointer()->getName()
                 << "\n");

    // If Source DFNode is a dummyNode, edge is from parent. Get the
    // argument from argument list of this internal node
    Value *inputVal;
    if (SrcDF->isEntryNode()) {
      inputVal = getArgumentAt(F_CPU, i);
      DEBUG(errs() << "Argument " << i << " = " << *inputVal << "\n");
    } else {
      // edge is from a internal node
      // Check - code should already be generated for this source dfnode
      // FIXME: Since the 2-level kernel code gen has aspecific structure, we
      // can assume the SrcDF is same as Kernel Leaf node.
      // Use outArgMap to get correct mapping
      SrcDF = K->KernelLeafNode;
      assert(OutputMap.count(SrcDF) &&
             "Source node call not found. Dependency violation!");

      // Find Output Value associated with the Source DFNode using OutputMap
      Value *CI = OutputMap[SrcDF];

      // Extract element at source position from this call instruction
      std::vector<unsigned> IndexList;
      // i is the destination of DFEdge E
      // Use the mapping instead of the bindings
      //      IndexList.push_back(E->getSourcePosition());
      IndexList.push_back(outArgMap[i]);
      DEBUG(errs() << "Going to generate ExtarctVal inst from " << *CI << "\n");
      ExtractValueInst *EI = ExtractValueInst::Create(CI, IndexList, "", RI);
      inputVal = EI;
    }
    std::vector<unsigned> IdxList;
    IdxList.push_back(i);
    retVal = InsertValueInst::Create(retVal, inputVal, IdxList, "", RI);
  }

  DEBUG(errs() << "Extracted all\n");
  switchToTimer(hpvm_TimerID_NONE, RI);
  retVal->setName("output");
  ReturnInst *newRI = ReturnInst::Create(F_CPU->getContext(), retVal);
  ReplaceInstWithInst(RI, newRI);
}

// Right now, only targeting the one level case. In general, device functions
// can return values so we don't need to change them
void CGT_OpenCL::codeGen(DFInternalNode *N) {
  DEBUG(errs() << "Inside internal node: " << N->getFuncPointer()->getName()
               << "\n");
  if (KernelLaunchNode == NULL)
    DEBUG(errs() << "No kernel launch node\n");
  else {
    DEBUG(errs() << "KernelLaunchNode: "
                 << KernelLaunchNode->getFuncPointer()->getName() << "\n");
  }

  if (!KernelLaunchNode) {
    DEBUG(errs()
          << "No code generated (host code for kernel launch complete).\n");
    return;
  }

  if (N == KernelLaunchNode) {
    DEBUG(errs() << "Found kernel launch node. Generating host code.\n");
    // TODO

    // Now the remaining nodes to be visited should be ignored
    KernelLaunchNode = NULL;
    DEBUG(errs() << "Insert Runtime calls\n");
    insertRuntimeCalls(N, kernel, getPTXFilename(M));

  } else {
    DEBUG(errs() << "Found intermediate node. Getting size parameters.\n");
    // Keep track of the arguments order.
    std::map<unsigned, unsigned> inmap1 = N->getInArgMap();
    std::map<unsigned, unsigned> inmap2 = kernel->getInArgMap();
    // TODO: Structure assumed: one thread node, one allocation node (at most),
    // TB node
    std::map<unsigned, unsigned> inmapFinal;
    for (std::map<unsigned, unsigned>::iterator ib = inmap2.begin(),
                                                ie = inmap2.end();
         ib != ie; ++ib) {
      inmapFinal[ib->first] = inmap1[ib->second];
    }

    kernel->setInArgMap(inmapFinal);

    // Keep track of the output arguments order.
    std::vector<unsigned> outmap1 = N->getOutArgMap();
    std::vector<unsigned> outmap2 = kernel->getOutArgMap();

    // TODO: Change when we have incoming edges to the dummy exit node from more
    // than one nodes. In this case, the number of bindings is the same, but
    // their destination position, thus the index in outmap1, is not
    // 0 ... outmap2.size()-1
    // The limit is the size of outmap2, because this is the number of kernel
    // output arguments for which the mapping matters
    // For now, it reasonable to assume that all the kernel arguments are
    // returned, maybe plys some others from other nodes, thus outmap2.size() <=
    // outmap1.size()
    for (unsigned i = 0; i < outmap2.size(); i++) {
      outmap1[i] = outmap2[outmap1[i]];
    }
    kernel->setOutArgMap(outmap1);

    // Track the source of local dimlimits for the kernel
    // Dimension limit can either be a constant or an argument of parent
    // function. Since Internal node would no longer exist, we need to insert
    // the localWGSize with values from the parent of N.
    std::vector<Value *> localWGSizeMapped;
    for (unsigned i = 0; i < kernel->localWGSize.size(); i++) {
      if (isa<Constant>(kernel->localWGSize[i])) {
        // if constant, use as it is
        localWGSizeMapped.push_back(kernel->localWGSize[i]);
      } else if (Argument *Arg = dyn_cast<Argument>(kernel->localWGSize[i])) {
        // if argument, find the argument location in N. Use InArgMap of N to
        // find the source location in Parent of N. Retrieve the argument from
        // parent to insert in the vector.
        unsigned argNum = Arg->getArgNo();
        // This argument will be coming from the parent node, not the allocation
        // Node
        assert(N->getInArgMap().find(argNum) != N->getInArgMap().end());

        unsigned parentArgNum = N->getInArgMap()[argNum];
        Argument *A =
            getArgumentAt(N->getParent()->getFuncPointer(), parentArgNum);
        localWGSizeMapped.push_back(A);
      } else {
        assert(
            false &&
            "LocalWGsize using value which is neither argument nor constant!");
      }
    }
    // Update localWGSize vector of kernel
    kernel->setLocalWGSize(localWGSizeMapped);
  }
}

void CGT_OpenCL::codeGen(DFLeafNode *N) {
  DEBUG(errs() << "Inside leaf node: " << N->getFuncPointer()->getName()
               << "\n");

  // Skip code generation if it is a dummy node
  if (N->isDummyNode()) {
    DEBUG(errs() << "Skipping dummy node\n");
    return;
  }

  // Skip code generation if it is an allocation node
  if (N->isAllocationNode()) {
    DEBUG(errs() << "Skipping allocation node\n");
    return;
  }

  // Generate code only if it has the right hint
  //  if(!checkPreferredTarget(N, hpvm::GPU_TARGET)) {
  //    errs() << "Skipping node: "<< N->getFuncPointer()->getName() << "\n";
  //    return;
  //  }
  if (!preferredTargetIncludes(N, hpvm::GPU_TARGET)) {
    DEBUG(errs() << "Skipping node: " << N->getFuncPointer()->getName()
                 << "\n");
    return;
  }

  // Checking which node is the kernel launch
  DFNode *PNode = N->getParent();
  int pLevel = PNode->getLevel();
  int pReplFactor = PNode->getNumOfDim();

  // Choose parent node as kernel launch if:
  // (1) Parent is the top level node i.e., Root of DFG
  //                    OR
  // (2) Parent does not have multiple instances
  DEBUG(errs() << "pLevel = " << pLevel << "\n");
  DEBUG(errs() << "pReplFactor = " << pReplFactor << "\n");
  assert((pLevel > 0) && "Root not allowed to be chosen as Kernel Node.");

  // Only these options are supported
  enum XLevelHierarchy { ONE_LEVEL, TWO_LEVEL } SelectedHierarchy;
  if (pLevel == 1 || !pReplFactor) {
    DEBUG(errs()
          << "*************** Kernel Gen: 1-Level Hierarchy **************\n");
    SelectedHierarchy = ONE_LEVEL;
    KernelLaunchNode = PNode;
    kernel = new Kernel(NULL, N, N->getInArgMap(), N->getSharedInArgMap(),
                        N->getOutArgMap(), N->getNumOfDim(), N->getDimLimits());
  } else {
    // Converting a 2-level DFG to opencl kernel
    DEBUG(errs()
          << "*************** Kernel Gen: 2-Level Hierarchy **************\n");
    assert((pLevel >= 2) &&
           "Selected node not nested deep enough to be Kernel Node.");
    SelectedHierarchy = TWO_LEVEL;
    KernelLaunchNode = PNode->getParent();
    assert((PNode->getNumOfDim() == N->getNumOfDim()) &&
           "Dimension number must match");
    // Contains the instructions generating the kernel configuration parameters
    kernel = new Kernel(NULL,             // kernel function
                        N,                // kernel leaf node
                        N->getInArgMap(), // kenel argument mapping
                        N->getSharedInArgMap(),
                        N->getOutArgMap(),     // kernel output mapping from the
                                               // leaf to the interemediate node
                        PNode->getNumOfDim(),  // gridDim
                        PNode->getDimLimits(), // grid size
                        N->getNumOfDim(),      // blockDim
                        N->getDimLimits());    // block size
  }

  std::vector<Instruction *> IItoRemove;
  BuildDFG::HandleToDFNode Leaf_HandleToDFNodeMap;

  // Get the function associated with the dataflow node
  Function *F = N->getFuncPointer();

  // Look up if we have visited this function before. If we have, then just
  // get the cloned function pointer from DFNode. Otherwise, create the cloned
  // function and add it to the DFNode GenFunc.
  //  Function *F_opencl = N->getGenFunc();
  Function *F_opencl = N->getGenFuncForTarget(hpvm::GPU_TARGET);

  assert(F_opencl == NULL &&
         "Error: Visiting a node for which code already generated");
  // Clone the function
  ValueToValueMapTy VMap;

  // F_opencl->setName(FName+"_opencl");

  Twine FName = F->getName();
  StringRef fStr = FName.getSingleStringRef();
  Twine newFName = Twine(fStr, "_opencl");
  F_opencl = CloneFunction(F, VMap);
  F_opencl->setName(newFName);

  //  errs() << "Old Function Name: " << F->getName() << "\n";
  //  errs() << "New Function Name: " << F_opencl->getName() << "\n";

  F_opencl->removeFromParent();

  // Insert the cloned function into the kernels module
  KernelM->getFunctionList().push_back(F_opencl);

  // TODO: Iterate over all the instructions of F_opencl and identify the
  // callees and clone them into this module.
  DEBUG(errs() << *F_opencl->getType());
  DEBUG(errs() << *F_opencl);

  // Transform  the function to void and remove all target dependent attributes
  // from the function
  F_opencl = transformFunctionToVoid(F_opencl);

  // Add generated function info to DFNode
  //  N->setGenFunc(F_opencl, hpvm::GPU_TARGET);
  N->addGenFunc(F_opencl, hpvm::GPU_TARGET, false);

  DEBUG(
      errs()
      << "Removing all attributes from Kernel Function and adding nounwind\n");
  F_opencl->removeAttributes(AttributeList::FunctionIndex,
                             F_opencl->getAttributes().getFnAttributes());
  F_opencl->addAttribute(AttributeList::FunctionIndex, Attribute::NoUnwind);

  // FIXME: For now, assume only one allocation node
  kernel->AllocationNode = NULL;

  for (DFNode::const_indfedge_iterator ieb = N->indfedge_begin(),
                                       iee = N->indfedge_end();
       ieb != iee; ++ieb) {
    DFNode *SrcDFNode = (*ieb)->getSourceDF();
    DEBUG(errs() << "Found edge from node: "
                 << " " << SrcDFNode->getFuncPointer()->getName() << "\n");
    DEBUG(errs() << "Current Node: " << N->getFuncPointer()->getName() << "\n");
    DEBUG(errs() << "isAllocationNode = " << SrcDFNode->isAllocationNode()
                 << "\n");
    if (!SrcDFNode->isDummyNode()) {
      assert(SrcDFNode->isAllocationNode());
      kernel->AllocationNode = dyn_cast<DFLeafNode>(SrcDFNode);
      kernel->allocInArgMap = SrcDFNode->getInArgMap();
      break;
    }
  }

  // Vector for shared memory arguments
  std::vector<unsigned> SharedMemArgs;

  // If no allocation node was found, SharedMemArgs is empty
  if (kernel->AllocationNode) {
    ValueToValueMapTy VMap;
    Function *F_alloc =
        CloneFunction(kernel->AllocationNode->getFuncPointer(), VMap);
    // F_alloc->removeFromParent();
    // Insert the cloned function into the kernels module
    // M.getFunctionList().push_back(F_alloc);

    std::vector<IntrinsicInst *> HPVMMallocInstVec;
    findIntrinsicInst(F_alloc, Intrinsic::hpvm_malloc, HPVMMallocInstVec);

    for (unsigned i = 0; i < HPVMMallocInstVec.size(); i++) {
      IntrinsicInst *II = HPVMMallocInstVec[i];
      assert(II->hasOneUse() && "hpvm_malloc result is used more than once");
      II->replaceAllUsesWith(
          ConstantPointerNull::get(Type::getInt8PtrTy(M.getContext())));
      II->eraseFromParent();
    }
    kernel->AllocationFunction = F_alloc;

    // This could be used to check that the allocation node has the appropriate
    // number of fields in its return struct
    /*
        ReturnInst *RI = ReturnInstVec[0];
        Value *RetVal = RI->getReturnValue();
        Type *RetTy = RetVal->getType();
        StructType *RetStructTy = dyn_cast<StructType>(RetTy);
        assert(RetStructTy && "Allocation node does not return a struct type");
        unsigned numFields = RetStructTy->getNumElements();
    */
    std::map<unsigned, std::pair<Value *, unsigned>> sharedInMap =
        kernel->getSharedInArgMap();
    AllocationNodeProperty *APN =
        (AllocationNodeProperty *)kernel->AllocationNode->getProperty(
            DFNode::Allocation);
    for (auto &AllocPair : APN->getAllocationList()) {
      unsigned destPos = AllocPair.first->getDestPosition();
      unsigned srcPos = AllocPair.first->getSourcePosition();
      SharedMemArgs.push_back(destPos);
      sharedInMap[destPos] =
          std::pair<Value *, unsigned>(AllocPair.second, srcPos + 1);
      sharedInMap[destPos + 1] =
          std::pair<Value *, unsigned>(AllocPair.second, srcPos + 1);
    }
    kernel->setSharedInArgMap(sharedInMap);
  }
  std::sort(SharedMemArgs.begin(), SharedMemArgs.end());

  // All pointer args which are not shared memory pointers have to be moved to
  // global address space
  unsigned argIndex = 0;
  std::vector<unsigned> GlobalMemArgs;
  for (Function::arg_iterator ai = F_opencl->arg_begin(),
                              ae = F_opencl->arg_end();
       ai != ae; ++ai) {
    if (ai->getType()->isPointerTy()) {
      // If the arguement is already chosen for shared memory arguemnt list,
      // skip. Else put it in Global memory arguement list
      if (std::count(SharedMemArgs.begin(), SharedMemArgs.end(), argIndex) ==
          0) {
        GlobalMemArgs.push_back(argIndex);
      }
    }
    argIndex++;
  }
  std::sort(GlobalMemArgs.begin(), GlobalMemArgs.end());

  /* At this point, we assume that chescks for the fact that SharedMemArgs only
     contains pointer arguments to GLOBAL_ADDRSPACE have been performed by the
     analysis pass */
  // Optimization: Gloabl memory arguments, which are not modified and whose
  // loads are not dependent on node id of current node, should be moved to
  // constant memory, subject to size of course
  std::vector<unsigned> ConstantMemArgs =
      globalToConstantMemoryOpt(&GlobalMemArgs, F_opencl);

  F_opencl = changeArgAddrspace(F_opencl, ConstantMemArgs, GLOBAL_ADDRSPACE);
  F_opencl = changeArgAddrspace(F_opencl, SharedMemArgs, SHARED_ADDRSPACE);
  F_opencl = changeArgAddrspace(F_opencl, GlobalMemArgs, GLOBAL_ADDRSPACE);

  // Function to replace call instructions to functions in the kernel
  std::map<Function *, Function *> OrgToClonedFuncMap;
  std::vector<Function *> FuncToBeRemoved;
  auto CloneAndReplaceCall = [&](CallInst *CI, Function *OrgFunc) {
    Function *NewFunc;
    // Check if the called function has already been cloned before.
    auto It = OrgToClonedFuncMap.find(OrgFunc);
    if (It == OrgToClonedFuncMap.end()) {
      ValueToValueMapTy VMap;
      NewFunc = CloneFunction(OrgFunc, VMap);
      OrgToClonedFuncMap[OrgFunc] = NewFunc;
      FuncToBeRemoved.push_back(NewFunc);
    } else {
      NewFunc = (*It).second;
    }
    // Replace the calls to this function
    std::vector<Value *> args;
    for (unsigned i = 0; i < CI->getNumArgOperands(); i++) {
      args.push_back(CI->getArgOperand(i));
    }
    CallInst *Inst = CallInst::Create(
        NewFunc, args,
        OrgFunc->getReturnType()->isVoidTy() ? "" : CI->getName(), CI);
    CI->replaceAllUsesWith(Inst);
    IItoRemove.push_back(CI);
    return NewFunc;
  };

  // Go through all the instructions
  for (inst_iterator i = inst_begin(F_opencl), e = inst_end(F_opencl); i != e;
       ++i) {
    Instruction *I = &(*i);
    // Leaf nodes should not contain HPVM graph intrinsics or launch
    assert(!BuildDFG::isHPVMLaunchIntrinsic(I) &&
           "Launch intrinsic within a dataflow graph!");
    assert(!BuildDFG::isHPVMGraphIntrinsic(I) &&
           "HPVM graph intrinsic within a leaf dataflow node!");

    if (BuildDFG::isHPVMIntrinsic(I)) {
      IntrinsicInst *II = dyn_cast<IntrinsicInst>(I);
      IntrinsicInst *ArgII;
      DFNode *ArgDFNode;

      /************************ Handle HPVM Query intrinsics
       * ************************/

      switch (II->getIntrinsicID()) {
      /**************************** llvm.hpvm.getNode()
       * *****************************/
      case Intrinsic::hpvm_getNode: {
        DEBUG(errs() << F_opencl->getName() << "\t: Handling getNode\n");
        // add mapping <intrinsic, this node> to the node-specific map
        Leaf_HandleToDFNodeMap[II] = N;
        IItoRemove.push_back(II);
      } break;
      /************************* llvm.hpvm.getParentNode()
       * **************************/
      case Intrinsic::hpvm_getParentNode: {
        DEBUG(errs() << F_opencl->getName() << "\t: Handling getParentNode\n");
        // get the parent node of the arg node
        // get argument node
        ArgII = cast<IntrinsicInst>((II->getOperand(0))->stripPointerCasts());
        ArgDFNode = Leaf_HandleToDFNodeMap[ArgII];
        // get the parent node of the arg node
        // Add mapping <intrinsic, parent node> to the node-specific map
        // the argument node must have been added to the map, orelse the
        // code could not refer to it
        Leaf_HandleToDFNodeMap[II] = ArgDFNode->getParent();

        IItoRemove.push_back(II);
      } break;
      /*************************** llvm.hpvm.getNumDims()
       * ***************************/
      case Intrinsic::hpvm_getNumDims: {
        DEBUG(errs() << F_opencl->getName() << "\t: Handling getNumDims\n");
        // get node from map
        // get the appropriate field
        ArgII = cast<IntrinsicInst>((II->getOperand(0))->stripPointerCasts());
        ArgDFNode = Leaf_HandleToDFNodeMap[ArgII];
        int numOfDim = ArgDFNode->getNumOfDim();
        DEBUG(errs() << "\t  Got node dimension : " << numOfDim << "\n");
        IntegerType *IntTy = Type::getInt32Ty(KernelM->getContext());
        ConstantInt *numOfDimConstant =
            ConstantInt::getSigned(IntTy, (int64_t)numOfDim);

        // Replace the result of the intrinsic with the computed value
        II->replaceAllUsesWith(numOfDimConstant);

        IItoRemove.push_back(II);
      } break;
      /*********************** llvm.hpvm.getNodeInstanceID()
       * ************************/
      case Intrinsic::hpvm_getNodeInstanceID_x:
      case Intrinsic::hpvm_getNodeInstanceID_y:
      case Intrinsic::hpvm_getNodeInstanceID_z: {
        DEBUG(errs() << F_opencl->getName()
                     << "\t: Handling getNodeInstanceID\n"
                     << "\t: " << *II << "\n");
        ArgII = cast<IntrinsicInst>((II->getOperand(0))->stripPointerCasts());
        ArgDFNode = Leaf_HandleToDFNodeMap[ArgII];
        assert(ArgDFNode && "Arg node is NULL");
        // A leaf node always has a parent
        DFNode *ParentDFNode = ArgDFNode->getParent();
        assert(ParentDFNode && "Parent node of a leaf is NULL");

        // Get the number associated with the required dimension
        // FIXME: The order is important!
        // These three intrinsics need to be consecutive x,y,z
        uint64_t dim =
            II->getIntrinsicID() - Intrinsic::hpvm_getNodeInstanceID_x;
        assert((dim < 3) && "Invalid dimension argument");
        DEBUG(errs() << "\t  dimension = " << dim << "\n");

        // Argument of the function to be called
        ConstantInt *DimConstant =
            ConstantInt::get(Type::getInt32Ty(KernelM->getContext()), dim);
        // ArrayRef<Value *> Args(DimConstant);

        // The following is to find which function to call
        Function *OpenCLFunction;

        FunctionType *FT =
            FunctionType::get(Type::getInt64Ty(KernelM->getContext()),
                              Type::getInt32Ty(KernelM->getContext()), false);
        if (SelectedHierarchy == ONE_LEVEL && ArgDFNode == N) {
          // We only have one level in the hierarchy or the parent node is not
          // replicated. This indicates that the parent node is the kernel
          // launch, so we need to specify a global id.
          // We can translate this only if the argument is the current node
          // itself
          DEBUG(errs() << "Substitute with get_global_id()\n");
          DEBUG(errs() << *II << "\n");
          OpenCLFunction = cast<Function>(
              (KernelM->getOrInsertFunction(StringRef("get_global_id"), FT))
                  .getCallee());
        } else if (Leaf_HandleToDFNodeMap[ArgII] == N) {
          // DEBUG(errs() << "Here inside cond 2\n");
          // We are asking for this node's id with respect to its parent
          // this is a local id call
          OpenCLFunction = cast<Function>(
              (KernelM->getOrInsertFunction(StringRef("get_local_id"), FT))
                  .getCallee());
          // DEBUG(errs() << "exiting condition 2\n");
        } else if (Leaf_HandleToDFNodeMap[ArgII] == N->getParent()) {
          // We are asking for this node's parent's id with respect to its
          // parent: this is a group id call
          OpenCLFunction = cast<Function>(
              (KernelM->getOrInsertFunction(StringRef("get_group_id"), FT))
                  .getCallee());
        } else {
          DEBUG(errs() << N->getFuncPointer()->getName() << "\n");
          DEBUG(errs() << N->getParent()->getFuncPointer()->getName() << "\n");
          DEBUG(errs() << *II << "\n");

          assert(false && "Unable to translate getNodeInstanceID intrinsic");
        }

        // DEBUG(errs() << "Create call instruction, insert it before the
        // instrinsic\n"); DEBUG(errs() << "Function: " << *OpenCLFunction <<
        // "\n"); DEBUG(errs() << "Arguments size: " << Args.size() << "\n");
        // DEBUG(errs() << "Argument: " << Args[0] << "\n");
        // DEBUG(errs() << "Arguments: " << *DimConstant << "\n");
        // Create call instruction, insert it before the intrinsic and
        // replace the uses of the previous instruction with the new one
        CallInst *CI = CallInst::Create(OpenCLFunction, DimConstant, "", II);
        // DEBUG(errs() << "Replace uses\n");
        II->replaceAllUsesWith(CI);

        IItoRemove.push_back(II);
      } break;
      /********************** llvm.hpvm.getNumNodeInstances()
       * ***********************/
      case Intrinsic::hpvm_getNumNodeInstances_x:
      case Intrinsic::hpvm_getNumNodeInstances_y:
      case Intrinsic::hpvm_getNumNodeInstances_z: {
        // TODO: think about whether this is the best way to go there are hw
        // specific registers. therefore it is good to have the intrinsic but
        // then, why do we need to keep that info in the graph?  (only for the
        // kernel configuration during the call)

        DEBUG(errs() << F_opencl->getName()
                     << "\t: Handling getNumNodeInstances\n");
        ArgII = cast<IntrinsicInst>((II->getOperand(0))->stripPointerCasts());
        ArgDFNode = Leaf_HandleToDFNodeMap[ArgII];
        // A leaf node always has a parent
        DFNode *ParentDFNode = ArgDFNode->getParent();
        assert(ParentDFNode && "Parent node of a leaf is NULL");

        // Get the number associated with the required dimension
        // FIXME: The order is important!
        // These three intrinsics need to be consecutive x,y,z
        uint64_t dim =
            II->getIntrinsicID() - Intrinsic::hpvm_getNumNodeInstances_x;
        assert((dim < 3) && "Invalid dimension argument");
        DEBUG(errs() << "\t  dimension = " << dim << "\n");

        // Argument of the function to be called
        ConstantInt *DimConstant =
            ConstantInt::get(Type::getInt32Ty(KernelM->getContext()), dim);
        // ArrayRef<Value *> Args(DimConstant);

        // The following is to find which function to call
        Function *OpenCLFunction;
        FunctionType *FT =
            FunctionType::get(Type::getInt64Ty(KernelM->getContext()),
                              Type::getInt32Ty(KernelM->getContext()), false);

        if (N == ArgDFNode && SelectedHierarchy == ONE_LEVEL) {
          // We only have one level in the hierarchy or the parent node is not
          // replicated. This indicates that the parent node is the kernel
          // launch, so the instances are global_size (gridDim x blockDim)
          OpenCLFunction = cast<Function>(
              (KernelM->getOrInsertFunction(StringRef("get_global_size"), FT))
                  .getCallee());
        } else if (Leaf_HandleToDFNodeMap[ArgII] == N) {
          // We are asking for this node's instances
          // this is a local size (block dim) call
          OpenCLFunction = cast<Function>(
              (KernelM->getOrInsertFunction(StringRef("get_local_size"), FT))
                  .getCallee());
        } else if (Leaf_HandleToDFNodeMap[ArgII] == N->getParent()) {
          // We are asking for this node's parent's instances
          // this is a (global_size/local_size) (grid dim) call
          OpenCLFunction = cast<Function>(
              (KernelM->getOrInsertFunction(StringRef("get_num_groups"), FT))
                  .getCallee());
        } else {
          assert(false && "Unable to translate getNumNodeInstances intrinsic");
        }

        // Create call instruction, insert it before the intrinsic and
        // replace the uses of the previous instruction with the new one
        CallInst *CI = CallInst::Create(OpenCLFunction, DimConstant, "", II);
        II->replaceAllUsesWith(CI);

        IItoRemove.push_back(II);
      } break;
      case Intrinsic::hpvm_barrier: {
        DEBUG(errs() << F_opencl->getName() << "\t: Handling barrier\n");
        DEBUG(errs() << "Substitute with barrier()\n");
        DEBUG(errs() << *II << "\n");
        FunctionType *FT = FunctionType::get(
            Type::getVoidTy(KernelM->getContext()),
            std::vector<Type *>(1, Type::getInt32Ty(KernelM->getContext())),
            false);
        Function *OpenCLFunction = cast<Function>(
            (KernelM->getOrInsertFunction(StringRef("barrier"), FT))
                .getCallee());
        CallInst *CI =
            CallInst::Create(OpenCLFunction,
                             ArrayRef<Value *>(ConstantInt::get(
                                 Type::getInt32Ty(KernelM->getContext()), 1)),
                             "", II);
        II->replaceAllUsesWith(CI);
        IItoRemove.push_back(II);
      } break;
      case Intrinsic::hpvm_atomic_add:
      case Intrinsic::hpvm_atomic_sub:
      case Intrinsic::hpvm_atomic_xchg:
      case Intrinsic::hpvm_atomic_min:
      case Intrinsic::hpvm_atomic_max:
      case Intrinsic::hpvm_atomic_and:
      case Intrinsic::hpvm_atomic_or:
      case Intrinsic::hpvm_atomic_xor: {
        DEBUG(errs() << *II << "\n");
        // Only have support for i32 atomic intrinsics
        assert(II->getType() == Type::getInt32Ty(II->getContext()) &&
               "Only support i32 atomic intrinsics for now");
        // Substitute with atomicrmw instruction
        assert(II->getNumArgOperands() == 2 &&
               "Expecting 2 operands for these atomics");
        Value *Ptr = II->getArgOperand(0);
        Value *Val = II->getArgOperand(1);
        assert(
            Ptr->getType()->isPointerTy() &&
            "First argument of supported atomics is expected to be a pointer");
        PointerType *PtrTy = cast<PointerType>(Ptr->getType());
        PointerType *TargetTy =
            Type::getInt32PtrTy(II->getContext(), PtrTy->getAddressSpace());
        if (PtrTy != TargetTy) {
          Ptr = CastInst::CreatePointerCast(Ptr, TargetTy, "", II);
          PtrTy = TargetTy;
        }

        std::string name;
        if (II->getIntrinsicID() == Intrinsic::hpvm_atomic_add)
          name = "atomic_add";
        else if (II->getIntrinsicID() == Intrinsic::hpvm_atomic_sub)
          name = "atomic_sub";
        else if (II->getIntrinsicID() == Intrinsic::hpvm_atomic_xchg)
          name = "atomic_xchg";
        else if (II->getIntrinsicID() == Intrinsic::hpvm_atomic_min)
          name = "atomic_min";
        else if (II->getIntrinsicID() == Intrinsic::hpvm_atomic_max)
          name = "atomic_max";
        else if (II->getIntrinsicID() == Intrinsic::hpvm_atomic_and)
          name = "atomic_and";
        else if (II->getIntrinsicID() == Intrinsic::hpvm_atomic_or)
          name = "atomic_or";
        else if (II->getIntrinsicID() == Intrinsic::hpvm_atomic_xor)
          name = "atomic_xor";
        Type *paramTypes[] = {PtrTy, Val->getType()};
        FunctionType *AtomFuncT = FunctionType::get(
            II->getType(), ArrayRef<Type *>(paramTypes, 2), false);
        FunctionCallee AtomFunc = KernelM->getOrInsertFunction(name, AtomFuncT);

        Value *Params[] = {Ptr, Val};
        CallInst *AtomCI = CallInst::Create(
            AtomFunc, ArrayRef<Value *>(Params, 2), II->getName(), II);
        DEBUG(errs() << "Substitute with: " << *AtomCI << "\n");
        II->replaceAllUsesWith(AtomCI);
        IItoRemove.push_back(II);
      } break;
      default:
        llvm_unreachable("Unknown HPVM Intrinsic!");
        break;
      }

    } else if (MemCpyInst *MemCpyI = dyn_cast<MemCpyInst>(I)) {
      IRBuilder<> Builder(I);
      Value *Source = MemCpyI->getSource();
      Value *Destination = MemCpyI->getArgOperand(0)->stripPointerCasts();
      Value *Length = MemCpyI->getOperand(2);
      DEBUG(errs() << "Found memcpy instruction: " << *I << "\n");
      DEBUG(errs() << "Source: " << *Source << "\n");
      DEBUG(errs() << "Destination: " << *Destination << "\n");
      DEBUG(errs() << "Length: " << *Length << "\n");

      size_t memcpy_length;
      unsigned int memcpy_count;
      if (ConstantInt *CI = dyn_cast<ConstantInt>(Length)) {
        if (CI->getBitWidth() <= 64) {
          memcpy_length = CI->getSExtValue();
          DEBUG(errs() << "Memcpy lenght = " << memcpy_length << "\n");
          Type *Source_Type = Source->getType()->getPointerElementType();
          DEBUG(errs() << "Source Type : " << *Source_Type << "\n");
          memcpy_count =
              memcpy_length / (Source_Type->getPrimitiveSizeInBits() / 8);
          DEBUG(errs() << "Memcpy count = " << memcpy_count << "\n");
          if (GetElementPtrInst *sourceGEPI =
                  dyn_cast<GetElementPtrInst>(Source)) {
            if (GetElementPtrInst *destGEPI =
                    dyn_cast<GetElementPtrInst>(Destination)) {
              Value *SourcePtrOperand = sourceGEPI->getPointerOperand();
              Value *DestPtrOperand = destGEPI->getPointerOperand();
              for (unsigned i = 0; i < memcpy_count; ++i) {
                Constant *increment;
                LoadInst *newLoadI;
                StoreInst *newStoreI;
                // First, need to increment the correct index for both source
                // and dest This invluves checking to see how many indeces the
                // GEP has Assume for now only 1 or 2 are the viable options.

                std::vector<Value *> GEPlIndex;
                if (sourceGEPI->getNumIndices() == 1) {
                  Value *Index = sourceGEPI->getOperand(1);
                  increment = ConstantInt::get(Index->getType(), i, false);
                  Value *incAdd = Builder.CreateAdd(Index, increment);
                  DEBUG(errs() << "Add: " << *incAdd << "\n");
                  GEPlIndex.push_back(incAdd);
                  Value *newGEPIl = Builder.CreateGEP(
                      SourcePtrOperand, ArrayRef<Value *>(GEPlIndex));
                  DEBUG(errs() << "Load GEP: " << *newGEPIl << "\n");
                  newLoadI = Builder.CreateLoad(newGEPIl);
                  DEBUG(errs() << "Load: " << *newLoadI << "\n");
                } else {
                  llvm_unreachable("Unhandled case where source GEPI has more "
                                   "than 1 indices!\n");
                }

                std::vector<Value *> GEPsIndex;
                if (destGEPI->getNumIndices() == 1) {

                } else if (destGEPI->getNumIndices() == 2) {
                  Value *Index0 = destGEPI->getOperand(1);
                  GEPsIndex.push_back(Index0);
                  Value *Index1 = destGEPI->getOperand(2);
                  increment = ConstantInt::get(Index1->getType(), i, false);
                  Value *incAdd = Builder.CreateAdd(Index1, increment);
                  DEBUG(errs() << "Add: " << *incAdd << "\n");
                  GEPsIndex.push_back(incAdd);
                  Value *newGEPIs = Builder.CreateGEP(
                      DestPtrOperand, ArrayRef<Value *>(GEPsIndex));
                  DEBUG(errs() << "Store GEP: " << *newGEPIs << "\n");
                  newStoreI = Builder.CreateStore(newLoadI, newGEPIs,
                                                  MemCpyI->isVolatile());
                  DEBUG(errs() << "Store: " << *newStoreI << "\n");
                } else {
                  llvm_unreachable("Unhandled case where dest GEPI has more "
                                   "than 2 indices!\n");
                }
              }
              IItoRemove.push_back(sourceGEPI);
              IItoRemove.push_back(destGEPI);
              Instruction *destBitcastI =
                  dyn_cast<Instruction>(MemCpyI->getArgOperand(0));
              Instruction *sourceBitcastI =
                  dyn_cast<Instruction>(MemCpyI->getArgOperand(1));
              IItoRemove.push_back(destBitcastI);
              IItoRemove.push_back(sourceBitcastI);
              IItoRemove.push_back(MemCpyI);
            }
          }
        }
      } else {
        llvm_unreachable("MEMCPY length is not a constant, not handled!\n");
      }
      //      llvm_unreachable("HERE!");
    }

    else if (CallInst *CI = dyn_cast<CallInst>(I)) {
      DEBUG(errs() << "Found a call: " << *CI << "\n");
      Function *calleeF =
          cast<Function>(CI->getCalledValue()->stripPointerCasts());
      if (calleeF->isDeclaration()) {
        // Add the declaration to kernel module
        if (calleeF->getName() == "sqrtf") {
          calleeF->setName(Twine("sqrt"));
          DEBUG(errs() << "CaleeF: " << *calleeF << "\n");
          DEBUG(errs() << "CI: " << *CI << "\n");
        } else if (calleeF->getName() == "rsqrtf") {
          calleeF->setName(Twine("rsqrt"));
          DEBUG(errs() << "CaleeF: " << *calleeF << "\n");
          DEBUG(errs() << "CI: " << *CI << "\n");
        }
        DEBUG(errs() << "Adding declaration to Kernel module: " << *calleeF
                     << "\n");
        KernelM->getOrInsertFunction(calleeF->getName(),
                                     calleeF->getFunctionType());
      } else {
        // Check if the called function has already been cloned before.
        Function *NewFunc = CloneAndReplaceCall(CI, calleeF);
        // Iterate over the new function to see if it calls any other functions
        // in the module.
        for (inst_iterator i = inst_begin(NewFunc), e = inst_end(NewFunc);
             i != e; ++i) {
          if (auto *Call = dyn_cast<CallInst>(&*i)) {
            Function *CalledFunc =
                cast<Function>(Call->getCalledValue()->stripPointerCasts());
            CloneAndReplaceCall(Call, CalledFunc);
          }
        }
      }
      // TODO: how to handle address space qualifiers in load/store
    }
  }
  // search for pattern where float is being casted to int and loaded/stored and
  // change it.
  DEBUG(errs() << "finding pattern for replacement!\n");
  for (inst_iterator i = inst_begin(F_opencl), e = inst_end(F_opencl); i != e;
       ++i) {
    bool cont = false;
    bool keepGEPI = false;
    bool keepGEPI2 = false;
    Instruction *I = &(*i);
    GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(I);

    if (!GEPI) {
      // did nod find pattern start, continue
      continue;
    }
    // may have found pattern, check
    DEBUG(errs() << "GEPI " << *GEPI << "\n");
    // print whatever we want for debug
    Value *PtrOp = GEPI->getPointerOperand();
    Type *SrcTy = GEPI->getSourceElementType();
    unsigned GEPIaddrspace = GEPI->getAddressSpace();

    if (SrcTy->isArrayTy())
      DEBUG(errs() << *SrcTy << " is an array type! "
                   << *(SrcTy->getArrayElementType()) << "\n");
    else
      DEBUG(errs() << *SrcTy << " is not an array type!\n");
    // check that source element type is float
    if (SrcTy->isArrayTy()) {
      if (!(SrcTy->getArrayElementType()->isFloatTy())) {
        DEBUG(errs() << "GEPI type is array but not float!\n");
        continue;
      }
    } else if (!(SrcTy->isFPOrFPVectorTy() /*isFloatTy()*/)) {
      DEBUG(errs() << "GEPI type is " << *SrcTy << "\n");
      // does not fit this pattern - no float GEP instruction
      continue;
    }
    // check that addressspace is 1
    //	  if (GEPIaddrspace != 1) {
    //			// does not fit this pattern - addrspace of pointer
    // argument is not global 			continue;
    //		}
    if (!(GEPI->hasOneUse())) {
      // does not fit this pattern - more than one uses
      // continue;
      // Keep GEPI around if it has other uses
      keepGEPI = true;
    }
    DEBUG(errs() << "Found GEPI " << *GEPI << "\n");

    // 1st GEPI it has one use
    //		assert(GEPI->hasOneUse() && "GEPI has a single use");

    // See if it is a bitcast
    BitCastInst *BitCastI;
    for (User *U : GEPI->users()) {
      if (Instruction *ui = dyn_cast<Instruction>(U)) {
        DEBUG(errs() << "--" << *ui << "\n");
        if (isa<BitCastInst>(ui)) {
          BitCastI = dyn_cast<BitCastInst>(ui);
          DEBUG(errs() << "---Found bitcast as only use of GEP\n");
          break;
        }
      }
      DEBUG(errs() << "GEPI does not have a bitcast user, continue\n");
      cont = true;
    }
    //		for (Value::user_iterator ui = GEPI->user_begin(),
    //				ue = GEPI->user_end(); ui!=ue; ++ui) {
    //        DEBUG(errs() << "--" << *ui << "\n");
    //			if (isa<BitCastInst>(*ui)) {
    //				BitCastI = dyn_cast<BitCastInst>(*ui);
    //        DEBUG(errs() << "Found bitcast as only use of GEP\n");
    //			}
    //		}

    if (cont /*!BitCastI*/) {
      continue; // not in pattern
    }

    //    DEBUG(errs() << *BitCastI << "\n");
    // Otherwise, check that first operand is GEP and 2nd is i32*. 1st Operand
    // has to be the GEP, since this is a use of the GEP.
    Value *Op2 = BitCastI->getOperand(0);
    DEBUG(errs() << "----" << *Op2 << "\n");
    //		assert(cast<Type>(Op2) && "Invalid Operand for Bitcast\n");
    //		Type *OpTy = cast<Type>(Op2);
    Type *OpTy = BitCastI->getDestTy();
    DEBUG(errs() << "---- Bitcast destination type: " << *OpTy << "\n");
    //    DEBUG(errs() << "---- " << *(Type::getInt32PtrTy(M.getContext(),1)) <<
    //    "\n");
    if (!(OpTy == Type::getInt32PtrTy(M.getContext(), GEPIaddrspace))) {
      // maybe right syntax is (Type::getInt32Ty)->getPointerTo()
      continue; // not in pattern
    }

    DEBUG(errs() << "----Here!\n");
    // We are in GEP, bitcast.

    // user_iterator, to find the load.

    if (!(BitCastI->hasOneUse())) {
      // does not fit this pattern - more than one uses
      continue;
    }
    DEBUG(errs() << "----Bitcast has one use!\n");
    // it has one use
    assert(BitCastI->hasOneUse() && "BitCastI has a single use");
    LoadInst *LoadI;
    for (User *U : BitCastI->users()) {
      if (Instruction *ui = dyn_cast<Instruction>(U)) {
        DEBUG(errs() << "-----" << *ui << "\n");
        if (isa<LoadInst>(ui)) {
          LoadI = dyn_cast<LoadInst>(ui);
          DEBUG(errs() << "-----Found load as only use of bitcast\n");
          break;
        }
      }
      DEBUG(errs() << "Bitcast does not have a load user, continue!\n");
      cont = true;
    }
    //		for (Value::user_iterator ui = BitCastI->user_begin(),
    //				ue = BitCastI->user_end(); ui!=ue; ++ui) {
    //			if (isa<LoadInst>(*ui)) {
    //				LoadI = dyn_cast<LoadInst>(*ui);
    //        errs() << "Found load as only use of bitcast\n";
    //			}
    //		}

    if (cont) {
      continue; // not in pattern
    }

    // check that we load from pointer we got from bitcast - assert - the unique
    // argument must be the use we found it from
    assert(LoadI->getPointerOperand() == BitCastI &&
           "Unexpected Load Instruction Operand\n");

    // Copy user_iterator, to find the store.

    if (!(LoadI->hasOneUse())) {
      // does not fit this pattern - more than one uses
      continue;
      // TODO: generalize: one load can have more than one store users
    }

    // it has one use
    assert(LoadI->hasOneUse() && "LoadI has a single use");
    Value::user_iterator ui = LoadI->user_begin();
    // skipped loop, because is has a single use
    StoreInst *StoreI = dyn_cast<StoreInst>(*ui);
    if (!StoreI) {
      continue; // not in pattern
    }

    // Also check that the store uses the loaded value as the value operand
    if (StoreI->getValueOperand() != LoadI) {
      continue;
    }

    DEBUG(errs() << "-------Found store instruction\n");

    // Look for its bitcast, which is its pointer operand
    Value *StPtrOp = StoreI->getPointerOperand();
    DEBUG(errs() << "-------" << *StPtrOp << "\n");
    BitCastInst *BitCastI2 = dyn_cast<BitCastInst>(StPtrOp);
    DEBUG(errs() << "-------" << *BitCastI2 << "\n");
    if (!BitCastI2) {
      continue; // not in pattern
    }

    DEBUG(errs() << "-------- Found Bit Cast of store!\n");
    // found bitcast. Look for the second GEP, its from operand.
    Value *BCFromOp = BitCastI2->getOperand(0);
    GetElementPtrInst *GEPI2 = dyn_cast<GetElementPtrInst>(BCFromOp);
    DEBUG(errs() << "---------- " << *GEPI2 << "\n");
    if (!GEPI2) {
      continue; // not in pattern
    }

    if (!(GEPI2->hasOneUse())) {
      // does not fit this pattern - more than one uses
      // continue;
      // Keep GEPI around if it has other uses
      keepGEPI2 = true;
    }
    DEBUG(errs() << "---------- Found GEPI of Bitcast!\n");

    Value *PtrOp2 = GEPI2->getPointerOperand();

    // Found GEPI2. TODO: kind of confused as o what checks I need to add here,
    // let's add them together- all the code for int-float type checks is
    // already above.

    // Assume we found pattern
    if (!keepGEPI) {
      IItoRemove.push_back(GEPI);
      DEBUG(errs() << "Pushing " << *GEPI << " for removal\n");
    } else {
      DEBUG(errs() << "Keeping " << *GEPI << " since it has multiple uses!\n");
    }
    IItoRemove.push_back(BitCastI);
    DEBUG(errs() << "Pushing " << *BitCastI << " for removal\n");
    IItoRemove.push_back(LoadI);
    DEBUG(errs() << "Pushing " << *LoadI << " for removal\n");
    IItoRemove.push_back(GEPI2);
    DEBUG(errs() << "Pushing " << *GEPI2 << " for removal\n");
    IItoRemove.push_back(BitCastI2);
    DEBUG(errs() << "Pushing " << *BitCastI2 << " for removal\n");
    if (!keepGEPI2) {
      IItoRemove.push_back(StoreI);
      DEBUG(errs() << "Pushing " << *StoreI << " for removal\n");
    } else {

      DEBUG(errs() << "Keeping " << *StoreI
                   << " since it has multiple uses!\n");
    }

    std::vector<Value *> GEPlIndex;
    if (GEPI->hasIndices()) {
      for (auto ii = GEPI->idx_begin(); ii != GEPI->idx_end(); ++ii) {
        Value *Index = dyn_cast<Value>(&*ii);
        DEBUG(errs() << "GEP-1 Index: " << *Index << "\n");
        GEPlIndex.push_back(Index);
      }
    }
    //    ArrayRef<Value*> GEPlArrayRef(GEPlIndex);

    std::vector<Value *> GEPsIndex;
    if (GEPI2->hasIndices()) {
      for (auto ii = GEPI2->idx_begin(); ii != GEPI2->idx_end(); ++ii) {
        Value *Index = dyn_cast<Value>(&*ii);
        DEBUG(errs() << "GEP-2 Index: " << *Index << "\n");
        GEPsIndex.push_back(Index);
      }
    }
    //    ArrayRef<Value*> GEPsArrayRef(GEPlIndex);

    //    ArrayRef<Value*>(GEPI->idx_begin(), GEPI->idx_end());
    GetElementPtrInst *newlGEP = GetElementPtrInst::Create(
        GEPI->getSourceElementType(), // Type::getFloatTy(M.getContext()),
        PtrOp,                        // operand from 1st GEP
        ArrayRef<Value *>(GEPlIndex), Twine(), StoreI);
    DEBUG(errs() << "Adding: " << *newlGEP << "\n");
    // insert load before GEPI
    LoadInst *newLoadI =
        new LoadInst(Type::getFloatTy(M.getContext()),
                     newlGEP, // new GEP
                     Twine(), LoadI->isVolatile(), LoadI->getAlignment(),
                     LoadI->getOrdering(), LoadI->getSyncScopeID(), StoreI);
    DEBUG(errs() << "Adding: " << *newLoadI << "\n");
    // same for GEP for store, for store operand
    GetElementPtrInst *newsGEP = GetElementPtrInst::Create(
        GEPI2->getSourceElementType(), // Type::getFloatTy(M.getContext()),
        PtrOp2,                        // operand from 2nd GEP
        ArrayRef<Value *>(GEPsIndex), Twine(), StoreI);
    DEBUG(errs() << "Adding: " << *newsGEP << "\n");
    // insert store before GEPI
    StoreInst *newStoreI =
        new StoreInst(newLoadI,
                      newsGEP, // new GEP
                      StoreI->isVolatile(), StoreI->getAlignment(),
                      StoreI->getOrdering(), StoreI->getSyncScopeID(), StoreI);
    DEBUG(errs() << "Adding: " << *newStoreI << "\n");
  }

  // We need to do this explicitly: DCE pass will not remove them because we
  // have assumed theworst memory behaviour for these function calls
  // Traverse the vector backwards, otherwise definitions are deleted while
  // their subsequent uses are still around
  for (auto *I : reverse(IItoRemove)) {
    DEBUG(errs() << "Erasing: " << *I << "\n");
    I->eraseFromParent();
  }

  // Removed the cloned functions from the parent module into the new module
  for (auto *F : FuncToBeRemoved) {
    F->removeFromParent(); // TODO: MARIA check
    KernelM->getFunctionList().push_back(F);
  }

  addCLMetadata(F_opencl);
  kernel->KernelFunction = F_opencl;
  DEBUG(errs() << "Identified kernel - " << kernel->KernelFunction->getName()
               << "\n");
  DEBUG(errs() << *KernelM);

  return;
}

bool DFG2LLVM_OpenCL::runOnModule(Module &M) {
  DEBUG(errs() << "\nDFG2LLVM_OpenCL PASS\n");

  // Get the BuildDFG Analysis Results:
  // - Dataflow graph
  // - Maps from i8* hansles to DFNode and DFEdge
  BuildDFG &DFG = getAnalysis<BuildDFG>();

  // DFInternalNode *Root = DFG.getRoot();
  std::vector<DFInternalNode *> Roots = DFG.getRoots();

  // Visitor for Code Generation Graph Traversal
  CGT_OpenCL *CGTVisitor = new CGT_OpenCL(M, DFG);

  // Iterate over all the DFGs and produce code for each one of them
  for (auto rootNode : Roots) {
    // Initiate code generation for root DFNode
    CGTVisitor->visit(rootNode);
  }

  CGTVisitor->writeKernelsModule();

  // TODO: Edit module epilogue to remove the HPVM intrinsic declarations
  delete CGTVisitor;

  return true;
}

std::string CGT_OpenCL::getKernelsModuleName(Module &M) {
  std::string mid = M.getModuleIdentifier();
  return mid.append(".kernels.ll");
}

void CGT_OpenCL::fixValueAddrspace(Value *V, unsigned addrspace) {
  assert(isa<PointerType>(V->getType()) && "Value should be of Pointer Type!");
  PointerType *OldTy = cast<PointerType>(V->getType());
  PointerType *NewTy = PointerType::get(OldTy->getElementType(), addrspace);
  V->mutateType(NewTy);
  for (Value::user_iterator ui = V->user_begin(), ue = V->user_end(); ui != ue;
       ui++) {
    // Change all uses producing pointer type in same address space to new
    // addressspace.
    if (PointerType *PTy = dyn_cast<PointerType>((*ui)->getType())) {
      if (PTy->getAddressSpace() == OldTy->getAddressSpace()) {
        fixValueAddrspace(*ui, addrspace);
      }
    }
  }
}

std::vector<unsigned>
CGT_OpenCL::globalToConstantMemoryOpt(std::vector<unsigned> *GlobalMemArgs,
                                      Function *F) {
  std::vector<unsigned> ConstantMemArgs;
  for (Function::arg_iterator ai = F->arg_begin(), ae = F->arg_end(); ai != ae;
       ++ai) {
    Argument *arg = &*ai;
    std::vector<unsigned>::iterator pos = std::find(
        GlobalMemArgs->begin(), GlobalMemArgs->end(), arg->getArgNo());
    // It has to be a global memory argument to be promotable
    if (pos == GlobalMemArgs->end())
      continue;

    // Check if it can/should be promoted
    if (canBePromoted(arg, F)) {
      DEBUG(errs() << "Promoting << " << arg->getName()
                   << " to constant memory."
                   << "\n");
      ConstantMemArgs.push_back(arg->getArgNo());
      GlobalMemArgs->erase(pos);
    }
  }
  return ConstantMemArgs;
}

Function *CGT_OpenCL::changeArgAddrspace(Function *F,
                                         std::vector<unsigned> &Args,
                                         unsigned addrspace) {
  unsigned idx = 0;
  std::vector<Type *> ArgTypes;
  for (Function::arg_iterator ai = F->arg_begin(), ae = F->arg_end(); ai != ae;
       ++ai) {
    Argument *arg = &*ai;
    DEBUG(errs() << *arg << "\n");
    unsigned argno = arg->getArgNo();
    if ((idx < Args.size()) && (argno == Args[idx])) {
      fixValueAddrspace(arg, addrspace);
      idx++;
    }
    ArgTypes.push_back(arg->getType());
  }
  FunctionType *newFT = FunctionType::get(F->getReturnType(), ArgTypes, false);

  // F->mutateType(PTy);
  Function *newF = cloneFunction(F, newFT, false);
  replaceNodeFunctionInIR(*F->getParent(), F, newF);

  DEBUG(errs() << *newF->getFunctionType() << "\n" << *newF << "\n");
  return newF;
}

/* Add metadata to module KernelM, for OpenCL kernels */
void CGT_OpenCL::addCLMetadata(Function *F) {

  IRBuilder<> Builder(&*F->begin());

  SmallVector<Metadata *, 8> KernelMD;
  KernelMD.push_back(ValueAsMetadata::get(F));

  // TODO: There is additional metadata used by kernel files but we skip them as
  // they are not mandatory. In future they might be useful to enable
  // optimizations

  MDTuple *MDKernelNode = MDNode::get(KernelM->getContext(), KernelMD);
  NamedMDNode *MDN_kernels =
      KernelM->getOrInsertNamedMetadata("opencl.kernels");
  MDN_kernels->addOperand(MDKernelNode);

  KernelMD.push_back(MDString::get(KernelM->getContext(), "kernel"));
  // TODO: Replace 1 with the number of the kernel.
  // Add when support for multiple launces is added
  KernelMD.push_back(ValueAsMetadata::get(
      ConstantInt::get(Type::getInt32Ty(KernelM->getContext()), 1)));
  MDNode *MDNvvmAnnotationsNode = MDNode::get(KernelM->getContext(), KernelMD);
  NamedMDNode *MDN_annotations =
      KernelM->getOrInsertNamedMetadata("nvvm.annotations");
  MDN_annotations->addOperand(MDNvvmAnnotationsNode);
}

void CGT_OpenCL::writeKernelsModule() {

  // In addition to deleting all other functions, we also want to spiff it
  // up a little bit.  Do this now.
  legacy::PassManager Passes;

  DEBUG(errs() << "Writing to File --- ");
  DEBUG(errs() << getKernelsModuleName(M).c_str() << "\n");
  std::error_code EC;
  ToolOutputFile Out(getKernelsModuleName(M).c_str(), EC, sys::fs::F_None);
  if (EC) {
    DEBUG(errs() << EC.message() << '\n');
  }

  Passes.add(createPrintModulePass(Out.os()));

  Passes.run(*KernelM);

  // Declare success.
  Out.keep();
}

Function *CGT_OpenCL::transformFunctionToVoid(Function *F) {

  DEBUG(errs() << "Transforming function to void: " << F->getName() << "\n");
  // FIXME: Maybe do that using the Node?
  StructType *FRetTy = dyn_cast<StructType>(F->getReturnType());
  assert(FRetTy && "Return Type must always be a struct");

  // Keeps return statements, because we will need to replace them
  std::vector<ReturnInst *> RItoRemove;
  findReturnInst(F, RItoRemove);

  std::vector<Type *> RetArgTypes;
  std::vector<Argument *> RetArgs;
  std::vector<Argument *> Args;
  // Check for { } return struct, which means that the function returns void
  if (FRetTy->isEmptyTy()) {

    DEBUG(errs() << "\tFunction output struct is void\n");
    DEBUG(errs() << "\tNo parameters added\n");

    // Replacing return statements with others returning void
    for (auto *RI : RItoRemove) {
      ReturnInst::Create((F->getContext()), 0, RI);
      RI->eraseFromParent();
    }
    DEBUG(errs() << "\tChanged return statements to return void\n");
  } else {
    // The struct has return values, thus needs to be converted to parameter

    // Iterate over all element types of return struct and add arguments to the
    // function
    for (unsigned i = 0; i < FRetTy->getNumElements(); i++) {
      Argument *RetArg =
          new Argument(FRetTy->getElementType(i)->getPointerTo(), "ret_arg", F);
      RetArgs.push_back(RetArg);
      RetArgTypes.push_back(RetArg->getType());
      DEBUG(errs() << "\tCreated parameter: " << *RetArg << "\n");
    }

    DEBUG(errs() << "\tReplacing Return statements\n");
    // Replace return statements with extractValue and store instructions
    for (auto *RI : RItoRemove) {
      Value *RetVal = RI->getReturnValue();
      for (unsigned i = 0; i < RetArgs.size(); i++) {
        ExtractValueInst *EI = ExtractValueInst::Create(
            RetVal, ArrayRef<unsigned>(i), RetArgs[i]->getName() + ".val", RI);
        new StoreInst(EI, RetArgs[i], RI);
      }
      // assert(RetVal && "Return value should not be null at this point");
      // StructType* RetType = cast<StructType>(RetVal->getType());
      // assert(RetType && "Return type is not a struct");

      ReturnInst::Create((F->getContext()), 0, RI);
      RI->eraseFromParent();
    }
  }
  DEBUG(errs() << "\tReplaced return statements\n");

  // Create the argument type list with the added argument's type
  std::vector<Type *> ArgTypes;
  for (Function::const_arg_iterator ai = F->arg_begin(), ae = F->arg_end();
       ai != ae; ++ai) {
    ArgTypes.push_back(ai->getType());
  }
  for (auto *RATy : RetArgTypes) {
    ArgTypes.push_back(RATy);
  }

  // Creating Args vector to use in cloning!
  for (Function::arg_iterator ai = F->arg_begin(), ae = F->arg_end(); ai != ae;
       ++ai) {
    Args.push_back(&*ai);
  }
  for (auto *ai : RetArgs) {
    Args.push_back(ai);
  }

  // Adding new arguments to the function argument list, would not change the
  // function type. We need to change the type of this function to reflect the
  // added arguments
  Type *VoidRetType = Type::getVoidTy(F->getContext());
  FunctionType *newFT = FunctionType::get(VoidRetType, ArgTypes, F->isVarArg());

  // Change the function type
  // F->mutateType(PTy);
  Function *newF = cloneFunction(F, newFT, false, NULL, &Args);
  replaceNodeFunctionInIR(*F->getParent(), F, newF);
  // F->eraseFromParent();
  return newF;
}

/******************************************************************************
 *                              Helper functions                              *
 ******************************************************************************/
// Check if argument arg can be promoted to constant memory in Function F
// Condition:
// 1. No stores
// 2. Loads not dependent on getNodeInstanceID itrinsic

static bool findLoadStoreUses(Value *V, std::vector<Value *> *UseList,
                              std::vector<Value *> *VisitedList) {
  if (std::find(VisitedList->begin(), VisitedList->end(), V) !=
      VisitedList->end()) {
    DEBUG(errs() << "\tAlready visited value: " << *V << "\n");
    return false;
  }
  VisitedList->push_back(V);
  for (Value::user_iterator ui = V->user_begin(), ue = V->user_end(); ui != ue;
       ++ui) {
    Instruction *I = dyn_cast<Instruction>(*ui);
    if (!I) {
      // if use is not an instruction, then skip it
      continue;
    }
    DEBUG(errs() << "\t" << *I << "\n");
    if (isa<LoadInst>(I)) {
      DEBUG(errs() << "\tFound load instruction: " << *I << "\n");
      DEBUG(errs() << "\tAdd to use list: " << *V << "\n");
      UseList->push_back(V);
    } else if (isa<StoreInst>(I) || isa<AtomicRMWInst>(I)) {
      // found a store in use chain
      DEBUG(errs() << "Found store/atomicrmw instruction: " << *I << "\n");
      return true;
    } else if (BuildDFG::isHPVMIntrinsic(I)) {
      // If it is an atomic intrinsic, we found a store
      IntrinsicInst *II = dyn_cast<IntrinsicInst>(I);
      assert(II &&
             II->getCalledValue()->getName().startswith("llvm.hpvm.atomic") &&
             "Only hpvm atomic intrinsics can have an argument as input");
      return true;
    } else {
      DEBUG(errs() << "\tTraverse use chain of: " << *I << "\n");
      if (findLoadStoreUses(I, UseList, VisitedList))
        return true;
    }
  }
  return false;
}

static bool isDependentOnNodeInstanceID(Value *V,
                                        std::vector<Value *> *DependenceList) {
  if (std::find(DependenceList->begin(), DependenceList->end(), V) !=
      DependenceList->end()) {
    DEBUG(errs() << "\tAlready visited value: " << *V << "\n");
    return false;
  }
  DependenceList->push_back(V);
  // If not an instruction, then not dependent on node instance id
  if (!isa<Instruction>(V) || isa<Constant>(V)) {
    DEBUG(errs() << "\tStop\n");
    return false;
  }

  Instruction *I = cast<Instruction>(V);
  for (unsigned i = 0; i < I->getNumOperands(); i++) {
    Value *operand = I->getOperand(i);
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(operand)) {
      if ((II->getIntrinsicID() == Intrinsic::hpvm_getNodeInstanceID_x ||
           II->getIntrinsicID() == Intrinsic::hpvm_getNodeInstanceID_y ||
           II->getIntrinsicID() == Intrinsic::hpvm_getNodeInstanceID_z)) {
        Value *Node = II->getArgOperand(0);
        IntrinsicInst *GN = dyn_cast<IntrinsicInst>(Node);
        assert(
            GN &&
            "NodeInstanceID operande should be node/parent node intrinsic\n");
        if (GN->getIntrinsicID() == Intrinsic::hpvm_getNode) {
          DEBUG(errs() << "\tDependency found on Node instance ID: " << *II
                       << "\n");
          return true;
        }
      }
    }
    if (CmpInst *CI = dyn_cast<CmpInst>(operand)) {
      DEBUG(errs() << "Found compare instruction: " << *CI
                   << "\nNot following its dependency list\n");
      continue;
    }
    DEBUG(errs() << "\tTraverse the operand chain of: " << *operand << "\n");
    if (isDependentOnNodeInstanceID(operand, DependenceList)) {
      return true;
    }
  }
  return false;
}

// Function to check if argument arg can be changed to a constant memory pointer
static bool canBePromoted(Argument *arg, Function *F) {
  DEBUG(errs() << "OPT: Check if Argument " << *arg
               << " can be changed to constant memory\n");
  std::vector<Value *> UseList;
  std::vector<Value *> VisitedList;
  // recursively traverse use chain
  // if find a store instruction return false, everything fails, cannot be
  // promoted
  // if find a load instruction as use, add the GEP instruction to list
  bool foundStore = findLoadStoreUses(arg, &UseList, &VisitedList);
  if (foundStore == true)
    return false;
  // See that the GEP instructions are not dependent on getNodeInstanceID
  // intrinsic
  DEBUG(errs() << foundStore
               << "\tNo Store Instruction found. Check dependence on node "
                  "instance ID\n");
  std::vector<Value *> DependenceList;
  for (auto U : UseList) {
    if (isDependentOnNodeInstanceID(U, &DependenceList))
      return false;
  }
  DEBUG(errs() << "\tYes, Promotable to Constant Memory\n");
  return true;
}

// Calculate execute node parameters which include, number of diemnsions for
// dynamic instances of the kernel, local and global work group sizes.
static void getExecuteNodeParams(Module &M, Value *&workDim, Value *&LocalWGPtr,
                                 Value *&GlobalWGPtr, Kernel *kernel,
                                 ValueToValueMapTy &VMap, Instruction *IB) {

  // Assign number of dimenstions a constant value
  workDim = ConstantInt::get(Type::getInt32Ty(M.getContext()), kernel->gridDim);

  // If local work group size if null
  if (!kernel->hasLocalWG()) {
    LocalWGPtr = Constant::getNullValue(Type::getInt64PtrTy(M.getContext()));
  } else {
    for (unsigned i = 0; i < kernel->localWGSize.size(); i++) {
      if (isa<Argument>(kernel->localWGSize[i]))
        kernel->localWGSize[i] = VMap[kernel->localWGSize[i]];
    }
    LocalWGPtr =
        genWorkGroupPtr(M, kernel->localWGSize, VMap, IB, "LocalWGSize");
  }

  for (unsigned i = 0; i < kernel->globalWGSize.size(); i++) {
    if (isa<Argument>(kernel->globalWGSize[i]))
      kernel->globalWGSize[i] = VMap[kernel->globalWGSize[i]];
  }

  // For OpenCL, global work group size is the total bumber of instances in each
  // dimension. So, multiply local and global dim limits.
  std::vector<Value *> globalWGSizeInsts;
  if (kernel->hasLocalWG()) {
    for (unsigned i = 0; i < kernel->gridDim; i++) {
      BinaryOperator *MulInst =
          BinaryOperator::Create(Instruction::Mul, kernel->globalWGSize[i],
                                 kernel->localWGSize[i], "", IB);
      globalWGSizeInsts.push_back(MulInst);
    }
  } else {
    globalWGSizeInsts = kernel->globalWGSize;
  }
  GlobalWGPtr = genWorkGroupPtr(M, globalWGSizeInsts, VMap, IB, "GlobalWGSize");
  DEBUG(errs() << "Pointer to global work group: " << *GlobalWGPtr << "\n");
}

// CodeGen for allocating space for Work Group on stack and returning a pointer
// to its address
static Value *genWorkGroupPtr(Module &M, std::vector<Value *> WGSize,
                              ValueToValueMapTy &VMap, Instruction *IB,
                              const Twine &WGName) {
  Value *WGPtr;
  // Get int64_t and or ease of use
  Type *Int64Ty = Type::getInt64Ty(M.getContext());

  // Work Group type is [#dim x i64]
  Type *WGTy = ArrayType::get(Int64Ty, WGSize.size());
  // Allocate space of Global work group data on stack and get pointer to
  // first element.
  AllocaInst *WG = new AllocaInst(WGTy, 0, WGName, IB);
  WGPtr = BitCastInst::CreatePointerCast(WG, Int64Ty->getPointerTo(),
                                         WG->getName() + ".0", IB);
  Value *nextDim = WGPtr;
  DEBUG(errs() << *WGPtr << "\n");

  // Iterate over the number of dimensions and store the global work group
  // size in that dimension
  for (unsigned i = 0; i < WGSize.size(); i++) {
    DEBUG(errs() << *WGSize[i] << "\n");
    assert(WGSize[i]->getType()->isIntegerTy() &&
           "Dimension not an integer type!");

    if (WGSize[i]->getType() != Int64Ty) {
      // If number of dimensions are mentioned in any other integer format,
      // generate code to extend it to i64. We need to use the mapped value in
      // the new generated function, hence the use of VMap
      // FIXME: Why are we changing the kernel WGSize vector here?
      DEBUG(errs() << "Not i64. Zero extend required.\n");
      DEBUG(errs() << *WGSize[i] << "\n");
      CastInst *CI =
          BitCastInst::CreateIntegerCast(WGSize[i], Int64Ty, true, "", IB);
      DEBUG(errs() << "Bitcast done.\n");
      StoreInst *SI = new StoreInst(CI, nextDim, IB);
      DEBUG(errs() << "Zero extend done.\n");
      DEBUG(errs() << "\tZero extended work group size: " << *SI << "\n");
    } else {
      // Store the value representing work group size in ith dimension on
      // stack
      StoreInst *SI = new StoreInst(WGSize[i], nextDim, IB);

      DEBUG(errs() << "\t Work group size: " << *SI << "\n");
    }
    if (i + 1 < WGSize.size()) {
      // Move to next dimension
      GetElementPtrInst *GEP = GetElementPtrInst::Create(
          nullptr, nextDim, ArrayRef<Value *>(ConstantInt::get(Int64Ty, 1)),
          WG->getName() + "." + Twine(i + 1), IB);
      DEBUG(errs() << "\tPointer to next dimension on stack: " << *GEP << "\n");
      nextDim = GEP;
    }
  }
  return WGPtr;
}

// Get generated PTX binary name
static std::string getPTXFilename(const Module &M) {
  std::string moduleID = M.getModuleIdentifier();
  moduleID.append(".kernels.cl");
  return moduleID;
}

// Changes the data layout of the Module to be compiled with OpenCL backend
// TODO: Figure out when to call it, probably after duplicating the modules
static void changeDataLayout(Module &M) {
  std::string opencl32_layoutStr = "e-p:32:32-i64:64-v16:16-v32:32-n16:32:64";
  std::string opencl64_layoutStr = "e-i64:64-v16:16-v32:32-n16:32:64";

  if (TARGET_PTX == 32)
    M.setDataLayout(StringRef(opencl32_layoutStr));
  else if (TARGET_PTX == 64)
    M.setDataLayout(StringRef(opencl64_layoutStr));
  else
    assert(false && "Invalid PTX target");

  return;
}

static void changeTargetTriple(Module &M) {
  std::string opencl32_TargetTriple = "opencl--nvidiacl";
  std::string opencl64_TargetTriple = "opencl64--nvidiacl";

  if (TARGET_PTX == 32)
    M.setTargetTriple(StringRef(opencl32_TargetTriple));
  else if (TARGET_PTX == 64)
    M.setTargetTriple(StringRef(opencl64_TargetTriple));
  else
    assert(false && "Invalid PTX target");

  return;
}

// Helper function, populate a vector with all return statements in a function
static void findReturnInst(Function *F,
                           std::vector<ReturnInst *> &ReturnInstVec) {
  for (auto &BB : *F) {
    if (auto *RI = dyn_cast<ReturnInst>(BB.getTerminator()))
      ReturnInstVec.push_back(RI);
  }
}

// Helper function, populate a vector with all IntrinsicID intrinsics in a
// function
static void findIntrinsicInst(Function *F, Intrinsic::ID IntrinsicID,
                              std::vector<IntrinsicInst *> &IntrinsicInstVec) {
  for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
    Instruction *I = &(*i);
    IntrinsicInst *II = dyn_cast<IntrinsicInst>(I);
    if (II && II->getIntrinsicID() == IntrinsicID) {
      IntrinsicInstVec.push_back(II);
    }
  }
}

} // End of namespace

char DFG2LLVM_OpenCL::ID = 0;
static RegisterPass<DFG2LLVM_OpenCL> X("dfg2llvm-opencl",
		"Dataflow Graph to LLVM for OpenCL Pass",
		false /* does not modify the CFG */,
		true /* transformation,   *
					* not just analysis */);
