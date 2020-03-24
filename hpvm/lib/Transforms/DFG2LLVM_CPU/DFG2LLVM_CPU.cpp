//===-------------------------- DFG2LLVM_CPU.cpp --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass is responsible for generating code for host code and kernel code 
// for CPU target using HPVM dataflow graph.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "DFG2LLVM_CPU"
#include "SupportHPVM/DFG2LLVM.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#ifndef LLVM_BUILD_DIR
#error LLVM_BUILD_DIR is not defined
#endif

#define STR_VALUE(X) #X
#define STRINGIFY(X) STR_VALUE(X)
#define LLVM_BUILD_DIR_STR STRINGIFY(LLVM_BUILD_DIR)

using namespace llvm;
using namespace builddfg;
using namespace dfg2llvm;

// HPVM Command line option to use timer or not
static cl::opt<bool> HPVMTimer_CPU("hpvm-timers-cpu",
                                   cl::desc("Enable hpvm timers"));

namespace {

// DFG2LLVM_CPU - The first implementation.
struct DFG2LLVM_CPU : public DFG2LLVM {
  static char ID; // Pass identification, replacement for typeid
  DFG2LLVM_CPU() : DFG2LLVM(ID) {}

private:
  // Member variables

  // Functions

public:
  bool runOnModule(Module &M);
};

// Visitor for Code generation traversal (tree traversal for now)
class CGT_CPU : public CodeGenTraversal {

private:
  // Member variables

  FunctionCallee malloc;
  // HPVM Runtime API
  FunctionCallee llvm_hpvm_cpu_launch;
  FunctionCallee llvm_hpvm_cpu_wait;
  FunctionCallee llvm_hpvm_cpu_argument_ptr;

  FunctionCallee llvm_hpvm_streamLaunch;
  FunctionCallee llvm_hpvm_streamPush;
  FunctionCallee llvm_hpvm_streamPop;
  FunctionCallee llvm_hpvm_streamWait;
  FunctionCallee llvm_hpvm_createBindInBuffer;
  FunctionCallee llvm_hpvm_createBindOutBuffer;
  FunctionCallee llvm_hpvm_createEdgeBuffer;
  FunctionCallee llvm_hpvm_createLastInputBuffer;
  FunctionCallee llvm_hpvm_createThread;
  FunctionCallee llvm_hpvm_bufferPush;
  FunctionCallee llvm_hpvm_bufferPop;
  FunctionCallee llvm_hpvm_cpu_dstack_push;
  FunctionCallee llvm_hpvm_cpu_dstack_pop;
  FunctionCallee llvm_hpvm_cpu_getDimLimit;
  FunctionCallee llvm_hpvm_cpu_getDimInstance;

  // Functions
  std::vector<IntrinsicInst *> *getUseList(Value *LI);
  Value *addLoop(Instruction *I, Value *limit, const Twine &indexName = "");
  void addWhileLoop(Instruction *, Instruction *, Instruction *, Value *);
  Instruction *addWhileLoopCounter(BasicBlock *, BasicBlock *, BasicBlock *);
  Argument *getArgumentFromEnd(Function *F, unsigned offset);
  Value *getInValueAt(DFNode *Child, unsigned i, Function *ParentF_CPU,
                      Instruction *InsertBefore);
  void invokeChild_CPU(DFNode *C, Function *F_CPU, ValueToValueMapTy &VMap,
                       Instruction *InsertBefore);
  void invokeChild_PTX(DFNode *C, Function *F_CPU, ValueToValueMapTy &VMap,
                       Instruction *InsertBefore);
  StructType *getArgumentListStructTy(DFNode *);
  Function *createFunctionFilter(DFNode *C);
  void startNodeThread(DFNode *, std::vector<Value *>,
                       DenseMap<DFEdge *, Value *>, Value *, Value *,
                       Instruction *);
  Function *createLaunchFunction(DFInternalNode *);

  // Virtual Functions
  void init() {
    HPVMTimer = HPVMTimer_CPU;
    TargetName = "CPU";
  }
  void initRuntimeAPI();
  void codeGen(DFInternalNode *N);
  void codeGen(DFLeafNode *N);
  Function *codeGenStreamPush(DFInternalNode *N);
  Function *codeGenStreamPop(DFInternalNode *N);

public:
  // Constructor
  CGT_CPU(Module &_M, BuildDFG &_DFG) : CodeGenTraversal(_M, _DFG) {
    init();
    initRuntimeAPI();
  }

  void codeGenLaunch(DFInternalNode *Root);
  void codeGenLaunchStreaming(DFInternalNode *Root);
};

bool DFG2LLVM_CPU::runOnModule(Module &M) {
  DEBUG(errs() << "\nDFG2LLVM_CPU PASS\n");

  // Get the BuildDFG Analysis Results:
  // - Dataflow graph
  // - Maps from i8* hansles to DFNode and DFEdge
  BuildDFG &DFG = getAnalysis<BuildDFG>();

  // DFInternalNode *Root = DFG.getRoot();
  std::vector<DFInternalNode *> Roots = DFG.getRoots();
  // BuildDFG::HandleToDFNode &HandleToDFNodeMap = DFG.getHandleToDFNodeMap();
  // BuildDFG::HandleToDFEdge &HandleToDFEdgeMap = DFG.getHandleToDFEdgeMap();

  // Visitor for Code Generation Graph Traversal
  CGT_CPU *CGTVisitor = new CGT_CPU(M, DFG);

  // Iterate over all the DFGs and produce code for each one of them
  for (auto &rootNode : Roots) {
    // Initiate code generation for root DFNode
    CGTVisitor->visit(rootNode);
    // Go ahead and replace the launch intrinsic with pthread call, otherwise
    // return now.
    // TODO: Later on, we might like to do this in a separate pass, which would
    // allow us the flexibility to switch between complete static code
    // generation for DFG or having a customized runtime+scheduler

    // Do streaming code generation if root node is streaming. Usual otherwise
    if (rootNode->isChildGraphStreaming())
      CGTVisitor->codeGenLaunchStreaming(rootNode);
    else
      CGTVisitor->codeGenLaunch(rootNode);
  }

  delete CGTVisitor;
  return true;
}

// Initialize the HPVM runtime API. This makes it easier to insert these calls
void CGT_CPU::initRuntimeAPI() {

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
  DECLARE(llvm_hpvm_cpu_launch);
  DECLARE(malloc);
  DECLARE(llvm_hpvm_cpu_wait);
  DECLARE(llvm_hpvm_cpu_argument_ptr);
  DECLARE(llvm_hpvm_streamLaunch);
  DECLARE(llvm_hpvm_streamPush);
  DECLARE(llvm_hpvm_streamPop);
  DECLARE(llvm_hpvm_streamWait);
  DECLARE(llvm_hpvm_createBindInBuffer);
  DECLARE(llvm_hpvm_createBindOutBuffer);
  DECLARE(llvm_hpvm_createEdgeBuffer);
  DECLARE(llvm_hpvm_createLastInputBuffer);
  DECLARE(llvm_hpvm_createThread);
  DECLARE(llvm_hpvm_bufferPush);
  DECLARE(llvm_hpvm_bufferPop);
  DECLARE(llvm_hpvm_cpu_dstack_push);
  DECLARE(llvm_hpvm_cpu_dstack_pop);
  DECLARE(llvm_hpvm_cpu_getDimLimit);
  DECLARE(llvm_hpvm_cpu_getDimInstance);

  // Get or insert timerAPI functions as well if you plan to use timers
  initTimerAPI();

  // Insert init context in main
  Function *VI = M.getFunction("llvm.hpvm.init");
  assert(VI->getNumUses() == 1 && "__hpvm__init should only be used once");
  DEBUG(errs() << "Inserting cpu timer initialization\n");
  Instruction *I = cast<Instruction>(*VI->user_begin());
  initializeTimerSet(I);
  switchToTimer(hpvm_TimerID_NONE, I);
  // Insert print instruction at hpvm exit
  Function *VC = M.getFunction("llvm.hpvm.cleanup");
  assert(VC->getNumUses() == 1 && "__hpvm__cleanup should only be used once");

  DEBUG(errs() << "Inserting cpu timer print\n");
  printTimerSet(I);
}

/* Returns vector of all wait instructions
 */
std::vector<IntrinsicInst *> *CGT_CPU::getUseList(Value *GraphID) {
  std::vector<IntrinsicInst *> *UseList = new std::vector<IntrinsicInst *>();
  // It must have been loaded from memory somewhere
  for (Value::user_iterator ui = GraphID->user_begin(),
                            ue = GraphID->user_end();
       ui != ue; ++ui) {
    if (IntrinsicInst *waitI = dyn_cast<IntrinsicInst>(*ui)) {
      UseList->push_back(waitI);
    } else {
      llvm_unreachable("Error: Operation on Graph ID not supported!\n");
    }
  }
  return UseList;
}

/* Traverse the function argument list in reverse order to get argument at a
 * distance offset fromt he end of argument list of function F
 */
Argument *CGT_CPU::getArgumentFromEnd(Function *F, unsigned offset) {
  assert((F->getFunctionType()->getNumParams() >= offset && offset > 0) &&
         "Invalid offset to access arguments!");
  Function::arg_iterator e = F->arg_end();
  // Last element of argument iterator is dummy. Skip it.
  e--;
  Argument *arg;
  for (; offset != 0; e--) {
    offset--;
    arg = &*e;
  }
  return arg;
}

/* Add Loop around the instruction I
 * Algorithm:
 * (1) Split the basic block of instruction I into three parts, where the
 * middleblock/body would contain instruction I.
 * (2) Add phi node before instruction I. Add incoming edge to phi node from
 * predecessor
 * (3) Add increment and compare instruction to index variable
 * (4) Replace terminator/branch instruction of body with conditional branch
 * which loops over bidy if true and goes to end if false
 * (5) Update phi node of body
 */
void CGT_CPU::addWhileLoop(Instruction *CondBlockStart, Instruction *BodyStart,
                           Instruction *BodyEnd, Value *TerminationCond) {
  BasicBlock *Entry = CondBlockStart->getParent();
  BasicBlock *CondBlock = Entry->splitBasicBlock(CondBlockStart, "condition");
  BasicBlock *WhileBody = CondBlock->splitBasicBlock(BodyStart, "while.body");
  BasicBlock *WhileEnd = WhileBody->splitBasicBlock(BodyEnd, "while.end");

  // Replace the terminator instruction of conditional with new conditional
  // branch which goes to while.body if true and branches to while.end otherwise
  BranchInst *BI = BranchInst::Create(WhileEnd, WhileBody, TerminationCond);
  ReplaceInstWithInst(CondBlock->getTerminator(), BI);

  // While Body should jump to condition block
  BranchInst *UnconditionalBranch = BranchInst::Create(CondBlock);
  ReplaceInstWithInst(WhileBody->getTerminator(), UnconditionalBranch);
}

Instruction *CGT_CPU::addWhileLoopCounter(BasicBlock *Entry, BasicBlock *Cond,
                                          BasicBlock *Body) {
  Module *M = Entry->getParent()->getParent();
  Type *Int64Ty = Type::getInt64Ty(M->getContext());

  // Insert a PHI instruction at the beginning of the condition block
  Instruction *IB = Cond->getFirstNonPHI();
  PHINode *CounterPhi = PHINode::Create(Int64Ty, 2, "cnt", IB);

  ConstantInt *IConst =
      ConstantInt::get(Type::getInt64Ty(M->getContext()), 1, true);
  Instruction *CounterIncr =
      BinaryOperator::CreateNSW(Instruction::BinaryOps::Add, CounterPhi, IConst,
                                "cnt_incr", Body->getTerminator());

  // Set incoming values for Phi node
  IConst = ConstantInt::get(Type::getInt64Ty(M->getContext()), 0, true);
  CounterPhi->addIncoming(IConst, Entry);
  CounterPhi->addIncoming(CounterIncr, Body);

  // Return the pointer to the created PHI node in the corresponding argument
  return CounterPhi;
}

/* Add Loop around the instruction I
 * Algorithm:
 * (1) Split the basic block of instruction I into three parts, where the
 * middleblock/body would contain instruction I.
 * (2) Add phi node before instruction I. Add incoming edge to phi node from
 * predecessor
 * (3) Add increment and compare instruction to index variable
 * (4) Replace terminator/branch instruction of body with conditional branch
 * which loops over bidy if true and goes to end if false
 * (5) Update phi node of body
 */
Value *CGT_CPU::addLoop(Instruction *I, Value *limit, const Twine &indexName) {
  BasicBlock *Entry = I->getParent();
  BasicBlock *ForBody = Entry->splitBasicBlock(I, "for.body");

  BasicBlock::iterator i(I);
  ++i;
  Instruction *NextI = &*i;
  // Next Instruction should also belong to the same basic block as the basic
  // block will have a terminator instruction
  assert(NextI->getParent() == ForBody &&
         "Next Instruction should also belong to the same basic block!");
  BasicBlock *ForEnd = ForBody->splitBasicBlock(NextI, "for.end");

  // Add Phi Node for index variable
  PHINode *IndexPhi = PHINode::Create(Type::getInt64Ty(I->getContext()), 2,
                                      "index." + indexName, I);

  // Add incoming edge to phi
  IndexPhi->addIncoming(ConstantInt::get(Type::getInt64Ty(I->getContext()), 0),
                        Entry);
  // Increment index variable
  BinaryOperator *IndexInc = BinaryOperator::Create(
      Instruction::Add, IndexPhi,
      ConstantInt::get(Type::getInt64Ty(I->getContext()), 1),
      "index." + indexName + ".inc", ForBody->getTerminator());

  // Compare index variable with limit
  CmpInst *Cond =
      CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_ULT, IndexInc, limit,
                      "cond." + indexName, ForBody->getTerminator());

  // Replace the terminator instruction of for.body with new conditional
  // branch which loops over body if true and branches to for.end otherwise
  BranchInst *BI = BranchInst::Create(ForBody, ForEnd, Cond);
  ReplaceInstWithInst(ForBody->getTerminator(), BI);

  // Add incoming edge to phi node in body
  IndexPhi->addIncoming(IndexInc, ForBody);
  return IndexPhi;
}

// Returns a packed struct type. The structtype is created by packing the input
// types, output types and isLastInput buffer type. All the streaming
// inputs/outputs are converted to i8*, since this is the type of buffer
// handles.
StructType *CGT_CPU::getArgumentListStructTy(DFNode *C) {
  std::vector<Type *> TyList;
  // Input types
  Function *CF = C->getFuncPointer();
  for (Function::arg_iterator ai = CF->arg_begin(), ae = CF->arg_end();
       ai != ae; ++ai) {
    if (C->getInDFEdgeAt(ai->getArgNo())->isStreamingEdge())
      TyList.push_back(Type::getInt8PtrTy(CF->getContext()));
    else
      TyList.push_back(ai->getType());
  }
  // Output Types
  StructType *OutStructTy = cast<StructType>(CF->getReturnType());
  for (unsigned i = 0; i < OutStructTy->getNumElements(); i++) {
    // All outputs of a node are streaming edge
    assert(C->getOutDFEdgeAt(i)->isStreamingEdge() &&
           "All output edges of child node have to be streaming");
    TyList.push_back(Type::getInt8PtrTy(CF->getContext()));
  }
  // isLastInput buffer element
  TyList.push_back(Type::getInt8PtrTy(CF->getContext()));

  StructType *STy =
      StructType::create(CF->getContext(), TyList,
                         Twine("struct.thread." + CF->getName()).str(), true);
  return STy;
}

void CGT_CPU::startNodeThread(DFNode *C, std::vector<Value *> Args,
                              DenseMap<DFEdge *, Value *> EdgeBufferMap,
                              Value *isLastInputBuffer, Value *graphID,
                              Instruction *IB) {
  DEBUG(errs() << "Starting Pipeline for child node: "
               << C->getFuncPointer()->getName() << "\n");
  // Create a filter/pipeline function for the child node
  Function *C_Pipeline = createFunctionFilter(C);
  Function *CF = C->getFuncPointer();

  // Get module context and i32 0 constant, as they would be frequently used in
  // this function.
  LLVMContext &Ctx = IB->getParent()->getContext();
  Constant *IntZero = ConstantInt::get(Type::getInt32Ty(Ctx), 0);

  // Marshall arguments
  // Create a packed struct type with inputs of C followed by outputs and then
  // another i8* to indicate isLastInput buffer. Streaming inputs are replaced
  // by i8*
  //
  StructType *STy = getArgumentListStructTy(C);
  // Allocate the struct on heap *NOT* stack and bitcast i8* to STy*
  CallInst *CI =
      CallInst::Create(malloc, ArrayRef<Value *>(ConstantExpr::getSizeOf(STy)),
                       C->getFuncPointer()->getName() + ".inputs", IB);
  CastInst *Struct = BitCastInst::CreatePointerCast(
      CI, STy->getPointerTo(), CI->getName() + ".i8ptr", IB);
  // AllocaInst* AI = new AllocaInst(STy,
  // C->getFuncPointer()->getName()+".inputs", IB);
  // Insert elements in the struct
  DEBUG(errs() << "Marshall inputs for child node: "
               << C->getFuncPointer()->getName() << "\n");
  // Marshall Inputs
  for (unsigned i = 0; i < CF->getFunctionType()->getNumParams(); i++) {
    // Create constant int (i)
    Constant *Int_i = ConstantInt::get(Type::getInt32Ty(Ctx), i);
    // Get Element pointer instruction
    Value *GEPIndices[] = {IntZero, Int_i};
    GetElementPtrInst *GEP = GetElementPtrInst::Create(
        nullptr, Struct, ArrayRef<Value *>(GEPIndices, 2),
        Struct->getName() + ".arg_" + Twine(i), IB);
    DFEdge *E = C->getInDFEdgeAt(i);
    if (E->getSourceDF()->isEntryNode()) {
      // This is a Bind Input Edge
      if (E->isStreamingEdge()) {
        // Streaming Bind Input edge. Get buffer corresponding to it
        assert(EdgeBufferMap.count(E) &&
               "No mapping buffer for a Streaming Bind DFEdge!");
        new StoreInst(EdgeBufferMap[E], GEP, IB);
      } else {
        // Non-streaming Bind edge
        new StoreInst(Args[i], GEP, IB);
      }
    } else {
      // This is an edge between siblings.
      // This must be an streaming edge. As it is our assumption that all edges
      // between two nodes in a DFG are streaming.
      assert(EdgeBufferMap.count(E) &&
             "No mapping buffer for a Streaming DFEdge!");
      new StoreInst(EdgeBufferMap[E], GEP, IB);
    }
  }
  unsigned numInputs = CF->getFunctionType()->getNumParams();
  unsigned numOutputs = cast<StructType>(CF->getReturnType())->getNumElements();
  // Marshall Outputs
  DEBUG(errs() << "Marshall outputs for child node: "
               << C->getFuncPointer()->getName() << "\n");
  for (unsigned i = 0; i < numOutputs; i++) {
    // Create constant int (i+numInputs)
    Constant *Int_i = ConstantInt::get(Type::getInt32Ty(Ctx), i + numInputs);
    // Get Element pointer instruction
    Value *GEPIndices[] = {IntZero, Int_i};
    GetElementPtrInst *GEP = GetElementPtrInst::Create(
        nullptr, Struct, ArrayRef<Value *>(GEPIndices, 2),
        Struct->getName() + ".out_" + Twine(i), IB);
    DFEdge *E = C->getOutDFEdgeAt(i);
    assert(E->isStreamingEdge() &&
           "Output Edge must be streaming of all nodes");
    assert(EdgeBufferMap.count(E) &&
           "No mapping buffer for a Out Streaming DFEdge!");
    new StoreInst(EdgeBufferMap[E], GEP, IB);
  }
  // Marshall last argument. isLastInput buffer
  DEBUG(errs() << "Marshall isLastInput for child node: "
               << C->getFuncPointer()->getName() << "\n");
  // Create constant int (i+numInputs)
  Constant *Int_index =
      ConstantInt::get(Type::getInt32Ty(Ctx), numInputs + numOutputs);
  // Get Element pointer instruction
  Value *GEPIndices[] = {IntZero, Int_index};
  GetElementPtrInst *GEP = GetElementPtrInst::Create(
      nullptr, Struct, ArrayRef<Value *>(GEPIndices, 2),
      Struct->getName() + ".isLastInput", IB);
  new StoreInst(isLastInputBuffer, GEP, IB);

  // AllocaInst AI points to memory with all the arguments packed
  // Call runtime to create the thread with these arguments
  DEBUG(errs() << "Start Thread for child node: "
               << C->getFuncPointer()->getName() << "\n");
  // DEBUG(errs() << *llvm_hpvm_createThread << "\n");
  DEBUG(errs() << *graphID->getType() << "\n");
  DEBUG(errs() << *C_Pipeline->getType() << "\n");
  DEBUG(errs() << *Struct->getType() << "\n");
  // Bitcast AI to i8*
  CastInst *BI = BitCastInst::CreatePointerCast(Struct, Type::getInt8PtrTy(Ctx),
                                                Struct->getName(), IB);
  Value *CreateThreadArgs[] = {graphID, C_Pipeline, BI};
  CallInst::Create(llvm_hpvm_createThread,
                   ArrayRef<Value *>(CreateThreadArgs, 3), "", IB);
}

Function *CGT_CPU::createLaunchFunction(DFInternalNode *N) {
  DEBUG(errs() << "Generating Streaming Launch Function\n");
  // Get Function associated with Node N
  Function *NF = N->getFuncPointer();

  // Map from Streaming edge to buffer
  DenseMap<DFEdge *, Value *> EdgeBufferMap;

  /* Now we have all the necessary global declarations necessary to generate the
   * Launch function, pointer to which can be passed to pthread utils to execute
   * DFG. The Launch function has just one input: i8* data.addr
   * This is the address of the all the input data that needs to be passed to
   * this function. In our case it contains the input arguments of the Root
   * function in the correct order.
   * (1) Create an empty Launch function of type void (i8* args, i8* GraphID)
   * (2) Extract each of inputs from data.addr
   * (3) create Buffers for all the streaming edges
   *     - Put buffers in the context
   * (4) Go over each child node
   *     - marshall its arguments together (use buffers in place of streaming
   *       arguments)
   *     - Start the threads
   * (5) The return value from Root is stored in memory, pointer to which is
   * passed to pthread_exit call.
   */
  // (1) Create Launch Function of type void (i8* args, i8* GraphID)
  Type *i8Ty = Type::getInt8Ty(M.getContext());
  Type *ArgTypes[] = {i8Ty->getPointerTo(), i8Ty->getPointerTo()};
  FunctionType *LaunchFuncTy = FunctionType::get(
      Type::getVoidTy(NF->getContext()), ArrayRef<Type *>(ArgTypes, 2), false);
  Function *LaunchFunc = Function::Create(
      LaunchFuncTy, NF->getLinkage(), NF->getName() + ".LaunchFunction", &M);
  DEBUG(errs() << "Generating Code for Streaming Launch Function\n");
  // Give a name to the argument which is used pass data to this thread
  Argument *data = &*LaunchFunc->arg_begin();
  // NOTE-HS: Check correctness with Maria
  Argument *graphID = &*(LaunchFunc->arg_begin() + 1);
  data->setName("data.addr");
  graphID->setName("graphID");
  // Add a basic block to this empty function and a return null statement to it
  DEBUG(errs() << *LaunchFunc->getReturnType() << "\n");
  BasicBlock *BB =
      BasicBlock::Create(LaunchFunc->getContext(), "entry", LaunchFunc);
  ReturnInst *RI = ReturnInst::Create(LaunchFunc->getContext(), BB);

  DEBUG(errs() << "Created Empty Launch Function\n");

  // (2) Extract each of inputs from data.addr
  std::vector<Type *> TyList;
  std::vector<std::string> names;
  std::vector<Value *> Args;

  for (Function::arg_iterator ai = NF->arg_begin(), ae = NF->arg_end();
       ai != ae; ++ai) {
    if (N->getChildGraph()
            ->getEntry()
            ->getOutDFEdgeAt(ai->getArgNo())
            ->isStreamingEdge()) {
      TyList.push_back(i8Ty->getPointerTo());
      names.push_back(Twine(ai->getName() + "_buffer").str());
      continue;
    }
    TyList.push_back(ai->getType());
    names.push_back(ai->getName());
  }
  Args = extractElements(data, TyList, names, RI);
  DEBUG(errs() << "Launch function for " << NF->getName() << *LaunchFunc
               << "\n");
  // (3) Create buffers for all the streaming edges
  for (DFGraph::dfedge_iterator di = N->getChildGraph()->dfedge_begin(),
                                de = N->getChildGraph()->dfedge_end();
       di != de; ++di) {
    DFEdge *Edge = *di;
    DEBUG(errs() << *Edge->getType() << "\n");
    Value *size = ConstantExpr::getSizeOf(Edge->getType());
    Value *CallArgs[] = {graphID, size};
    if (Edge->isStreamingEdge()) {
      CallInst *CI;
      // Create a buffer call
      if (Edge->getSourceDF()->isEntryNode()) {
        // Bind Input Edge
        Constant *Int_ArgNo = ConstantInt::get(
            Type::getInt32Ty(RI->getContext()), Edge->getSourcePosition());
        Value *BindInCallArgs[] = {graphID, size, Int_ArgNo};
        CI = CallInst::Create(
            llvm_hpvm_createBindInBuffer, ArrayRef<Value *>(BindInCallArgs, 3),
            "BindIn." + Edge->getDestDF()->getFuncPointer()->getName(), RI);
      } else if (Edge->getDestDF()->isExitNode()) {
        // Bind Output Edge
        CI = CallInst::Create(
            llvm_hpvm_createBindOutBuffer, ArrayRef<Value *>(CallArgs, 2),
            "BindOut." + Edge->getSourceDF()->getFuncPointer()->getName(), RI);
      } else {
        // Streaming Edge
        CI = CallInst::Create(
            llvm_hpvm_createEdgeBuffer, ArrayRef<Value *>(CallArgs, 2),
            Edge->getSourceDF()->getFuncPointer()->getName() + "." +
                Edge->getDestDF()->getFuncPointer()->getName(),
            RI);
      }
      EdgeBufferMap[Edge] = CI;
    }
  }
  // Create buffer for isLastInput for all the child nodes
  DFGraph *G = N->getChildGraph();
  DenseMap<DFNode *, Value *> NodeLastInputMap;
  for (DFGraph::children_iterator ci = G->begin(), ce = G->end(); ci != ce;
       ++ci) {
    DFNode *child = *ci;
    if (child->isDummyNode())
      continue;
    Value *size = ConstantExpr::getSizeOf(Type::getInt64Ty(NF->getContext()));
    Value *CallArgs[] = {graphID, size};
    CallInst *CI = CallInst::Create(
        llvm_hpvm_createLastInputBuffer, ArrayRef<Value *>(CallArgs, 2),
        "BindIn.isLastInput." + child->getFuncPointer()->getName(), RI);
    NodeLastInputMap[child] = CI;
  }
  DEBUG(errs() << "Start Each child node filter\n");
  // (4) Marshall arguments for each child node and start the thread with its
  //     pipeline funtion
  for (DFGraph::children_iterator ci = N->getChildGraph()->begin(),
                                  ce = N->getChildGraph()->end();
       ci != ce; ++ci) {
    DFNode *C = *ci;
    // Skip dummy node call
    if (C->isDummyNode())
      continue;

    // Marshall all the arguments for this node into an i8*
    // Pass to the runtime to create the thread
    // Start the thread for child node C
    startNodeThread(C, Args, EdgeBufferMap, NodeLastInputMap[C], graphID, RI);
  }

  DEBUG(errs() << "Launch function:\n");
  DEBUG(errs() << *LaunchFunc << "\n");

  return LaunchFunc;
}

/* This fuction does the steps necessary to launch a streaming graph
 * Steps
 * Create Pipeline/Filter function for each node in child graph of Root
 * Create Functions DFGLaunch, DFGPush, DFGPop, DFGWait
 * Modify each of the instrinsic in host code
 * Launch, Push, Pop, Wait
 */
void CGT_CPU::codeGenLaunchStreaming(DFInternalNode *Root) {
  IntrinsicInst *LI = Root->getInstruction();
  Function *RootLaunch = createLaunchFunction(Root);
  // Substitute launch intrinsic main
  DEBUG(errs() << "Substitute launch intrinsic\n");
  Value *LaunchInstArgs[] = {RootLaunch, LI->getArgOperand(1)};
  CallInst *LaunchInst = CallInst::Create(
      llvm_hpvm_streamLaunch, ArrayRef<Value *>(LaunchInstArgs, 2),
      "graph" + Root->getFuncPointer()->getName(), LI);

  DEBUG(errs() << *LaunchInst << "\n");
  // Replace all wait instructions with cpu specific wait instructions
  DEBUG(errs() << "Substitute wait, push, pop intrinsics\n");
  std::vector<IntrinsicInst *> *UseList = getUseList(LI);
  for (unsigned i = 0; i < UseList->size(); ++i) {
    IntrinsicInst *II = UseList->at(i);
    CallInst *CI;
    Value *PushArgs[] = {LaunchInst, II->getOperand(1)};
    switch (II->getIntrinsicID()) {
    case Intrinsic::hpvm_wait:
      CI = CallInst::Create(llvm_hpvm_streamWait, ArrayRef<Value *>(LaunchInst),
                            "");
      break;
    case Intrinsic::hpvm_push:
      CI = CallInst::Create(llvm_hpvm_streamPush,
                            ArrayRef<Value *>(PushArgs, 2), "");
      break;
    case Intrinsic::hpvm_pop:
      CI = CallInst::Create(llvm_hpvm_streamPop, ArrayRef<Value *>(LaunchInst),
                            "");
      break;
    default:
      llvm_unreachable(
          "GraphID is used by an instruction other than wait, push, pop");
    };
    DEBUG(errs() << "Replace:\n\t" << *II << "\n");
    ReplaceInstWithInst(II, CI);
    DEBUG(errs() << "\twith " << *CI << "\n");
  }
}

void CGT_CPU::codeGenLaunch(DFInternalNode *Root) {
  // TODO: Place an assert to check if the constant passed by launch intrinsic
  // as the number of arguments to DFG is same as the number of arguments of the
  // root of DFG
  DEBUG(errs() << "Generating Launch Function\n");
  // Get Launch Instruction
  IntrinsicInst *LI = Root->getInstruction();
  switchToTimer(hpvm_TimerID_PTHREAD_CREATE, LI);
  DEBUG(errs() << "Generating Launch Function\n");

  /* Now we have all the necessary global declarations necessary to generate the
   * Launch function, pointer to which can be passed to pthread utils to execute
   * DFG. The Launch function has just one input: i8* data.addr
   * This is the address of the all the input data that needs to be passed to
   * this function. In our case it contains the input arguments of the Root
   * function in the correct order.
   * (1) Create an empty Launch function of type i8*(i8*)
   * (2) Extract each of inputs from data.addr and pass them as arguments to the
   * call to Root function
   * (3) The return value from Root is stored in memory, pointer to which is
   * passed to pthread_exit call.
   */
  // Create Launch Function of type i8*(i8*) which calls the root function
  Type *i8Ty = Type::getInt8Ty(M.getContext());
  FunctionType *AppFuncTy = FunctionType::get(
      i8Ty->getPointerTo(), ArrayRef<Type *>(i8Ty->getPointerTo()), false);
  Function *AppFunc =
      Function::Create(AppFuncTy, Root->getFuncPointer()->getLinkage(),
                       "LaunchDataflowGraph", &M);
  DEBUG(errs() << "Generating Launch Function\n");
  // Give a name to the argument which is used pass data to this thread
  Value *data = &*AppFunc->arg_begin();
  data->setName("data.addr");
  // Add a basic block to this empty function and a return null statement to it
  BasicBlock *BB = BasicBlock::Create(AppFunc->getContext(), "entry", AppFunc);
  ReturnInst *RI =
      ReturnInst::Create(AppFunc->getContext(),
                         Constant::getNullValue(AppFunc->getReturnType()), BB);
  switchToTimer(hpvm_TimerID_ARG_UNPACK, RI);

  DEBUG(errs() << "Created Empty Launch Function\n");
  // Find the CPU function generated for Root and
  //  Function* RootF_CPU = Root->getGenFunc();
  Function *RootF_CPU = Root->getGenFuncForTarget(hpvm::CPU_TARGET);
  assert(RootF_CPU && "Error: No generated CPU function for Root node\n");
  assert(Root->hasCPUGenFuncForTarget(hpvm::CPU_TARGET) &&
         "Error: Generated Function for Root node with no cpu wrapper\n");

  // Generate a call to RootF_CPU with null parameters for now
  std::vector<Value *> Args;
  for (unsigned i = 0; i < RootF_CPU->getFunctionType()->getNumParams(); i++) {
    Args.push_back(
        Constant::getNullValue(RootF_CPU->getFunctionType()->getParamType(i)));
  }
  CallInst *CI =
      CallInst::Create(RootF_CPU, Args, RootF_CPU->getName() + ".output", RI);

  // Extract input data from i8* data.addr and patch them to correct argument of
  // call to RootF_CPU. For each argument
  std::vector<Type *> TyList;
  std::vector<std::string> names;
  for (Function::arg_iterator ai = RootF_CPU->arg_begin(),
                              ae = RootF_CPU->arg_end();
       ai != ae; ++ai) {
    TyList.push_back(ai->getType());
    names.push_back(ai->getName());
  }
  std::vector<Value *> elements = extractElements(data, TyList, names, CI);
  // Patch the elements to the call arguments
  for (unsigned i = 0; i < CI->getNumArgOperands(); i++)
    CI->setArgOperand(i, elements[i]);

  // Add timers around Call to RootF_CPU function
  switchToTimer(hpvm_TimerID_COMPUTATION, CI);
  switchToTimer(hpvm_TimerID_OUTPUT_PACK, RI);

  StructType *RootRetTy =
      cast<StructType>(RootF_CPU->getFunctionType()->getReturnType());

  // if Root has non empty return
  if (RootRetTy->getNumElements()) {
    // We can't access the type of the arg struct - build it
    std::vector<Type *> TyList;
    for (Function::arg_iterator ai = RootF_CPU->arg_begin(),
                                ae = RootF_CPU->arg_end();
         ai != ae; ++ai) {
      TyList.push_back(ai->getType());
    }
    TyList.push_back(CI->getType());

    StructType *ArgStructTy = StructType::create(
        M.getContext(), ArrayRef<Type *>(TyList),
        (RootF_CPU->getName() + ".arg.struct.ty").str(), true);

    // Cast the data pointer to the type of the arg struct
    CastInst *OutputAddrCast = CastInst::CreatePointerCast(
        data, ArgStructTy->getPointerTo(), "argStructCast.addr", RI);

    // Result struct is the last element of the packed struct passed to launch
    unsigned outStructIdx = ArgStructTy->getNumElements() - 1;

    ConstantInt *IntZero =
        ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);
    ConstantInt *IntIdx =
        ConstantInt::get(Type::getInt32Ty(M.getContext()), outStructIdx);

    Value *GEPIIdxList[] = {IntZero, IntIdx};
    // Get data pointer to the last element of struct - result field
    GetElementPtrInst *OutGEPI = GetElementPtrInst::Create(
        ArgStructTy, OutputAddrCast, ArrayRef<Value *>(GEPIIdxList, 2),
        CI->getName() + ".addr", RI);
    // Store result there
    new StoreInst(CI, OutGEPI, RI);
  } else {
    // There is no return - no need to actually code gen, but for fewer
    // changes maintain what code was already doing
    // We were casting the data pointer to the result type of Root, and
    // returning result there. This would work at the LLVM level, but not
    // at the C level, thus the rewrite.
    CastInst *OutputAddrCast = CastInst::CreatePointerCast(
        data, CI->getType()->getPointerTo(), CI->getName() + ".addr", RI);
    new StoreInst(CI, OutputAddrCast, RI);
  }

  switchToTimer(hpvm_TimerID_NONE, RI);

  DEBUG(errs() << "Application specific function:\n");
  DEBUG(errs() << *AppFunc << "\n");

  // Substitute launch intrinsic main
  Value *LaunchInstArgs[] = {AppFunc, LI->getArgOperand(1)};
  CallInst *LaunchInst = CallInst::Create(
      llvm_hpvm_cpu_launch, ArrayRef<Value *>(LaunchInstArgs, 2),
      "graph" + Root->getFuncPointer()->getName(), LI);
  // ReplaceInstWithInst(LI, LaunchInst);

  DEBUG(errs() << *LaunchInst << "\n");
  // Replace all wait instructions with cpu specific wait instructions
  std::vector<IntrinsicInst *> *UseList = getUseList(LI);
  for (unsigned i = 0; i < UseList->size(); ++i) {
    IntrinsicInst *II = UseList->at(i);
    CallInst *CI;
    switch (II->getIntrinsicID()) {
    case Intrinsic::hpvm_wait:
      CI = CallInst::Create(llvm_hpvm_cpu_wait, ArrayRef<Value *>(LaunchInst),
                            "");
      break;
    case Intrinsic::hpvm_push:
      CI = CallInst::Create(llvm_hpvm_bufferPush, ArrayRef<Value *>(LaunchInst),
                            "");
      break;
    case Intrinsic::hpvm_pop:
      CI = CallInst::Create(llvm_hpvm_bufferPop, ArrayRef<Value *>(LaunchInst),
                            "");
      break;
    default:
      llvm_unreachable(
          "GraphID is used by an instruction other than wait, push, pop");
    };
    ReplaceInstWithInst(II, CI);
    DEBUG(errs() << *CI << "\n");
  }
}

Value *CGT_CPU::getInValueAt(DFNode *Child, unsigned i, Function *ParentF_CPU,
                             Instruction *InsertBefore) {
  // TODO: Assumption is that each input port of a node has just one
  // incoming edge. May change later on.

  // Find the incoming edge at the requested input port
  DFEdge *E = Child->getInDFEdgeAt(i);
  assert(E && "No incoming edge or binding for input element!");
  // Find the Source DFNode associated with the incoming edge
  DFNode *SrcDF = E->getSourceDF();

  // If Source DFNode is a dummyNode, edge is from parent. Get the
  // argument from argument list of this internal node
  Value *inputVal;
  if (SrcDF->isEntryNode()) {
    inputVal = getArgumentAt(ParentF_CPU, E->getSourcePosition());
    DEBUG(errs() << "Argument " << i << " = " << *inputVal << "\n");
  } else {
    // edge is from a sibling
    // Check - code should already be generated for this source dfnode
    assert(OutputMap.count(SrcDF) &&
           "Source node call not found. Dependency violation!");

    // Find CallInst associated with the Source DFNode using OutputMap
    Value *CI = OutputMap[SrcDF];

    // Extract element at source position from this call instruction
    std::vector<unsigned> IndexList;
    IndexList.push_back(E->getSourcePosition());
    DEBUG(errs() << "Going to generate ExtarctVal inst from " << *CI << "\n");
    ExtractValueInst *EI =
        ExtractValueInst::Create(CI, IndexList, "", InsertBefore);
    inputVal = EI;
  }
  return inputVal;
}

void CGT_CPU::invokeChild_CPU(DFNode *C, Function *F_CPU,
                              ValueToValueMapTy &VMap, Instruction *IB) {
  Function *CF = C->getFuncPointer();

  //  Function* CF_CPU = C->getGenFunc();
  Function *CF_CPU = C->getGenFuncForTarget(hpvm::CPU_TARGET);
  assert(CF_CPU != NULL &&
         "Found leaf node for which code generation has not happened yet!\n");
  assert(C->hasCPUGenFuncForTarget(hpvm::CPU_TARGET) &&
         "The generated function to be called from cpu backend is not an cpu "
         "function\n");
  DEBUG(errs() << "Invoking child node" << CF_CPU->getName() << "\n");

  std::vector<Value *> Args;
  // Create argument list to pass to call instruction
  // First find the correct values using the edges
  // The remaing six values are inserted as constants for now.
  for (unsigned i = 0; i < CF->getFunctionType()->getNumParams(); i++) {
    Args.push_back(getInValueAt(C, i, F_CPU, IB));
  }

  Value *I64Zero = ConstantInt::get(Type::getInt64Ty(F_CPU->getContext()), 0);
  for (unsigned j = 0; j < 6; j++)
    Args.push_back(I64Zero);

  DEBUG(errs() << "Gen Function type: " << *CF_CPU->getType() << "\n");
  DEBUG(errs() << "Node Function type: " << *CF->getType() << "\n");
  DEBUG(errs() << "Arguments: " << Args.size() << "\n");

  // Call the F_CPU function associated with this node
  CallInst *CI =
      CallInst::Create(CF_CPU, Args, CF_CPU->getName() + "_output", IB);
  DEBUG(errs() << *CI << "\n");
  OutputMap[C] = CI;

  // Find num of dimensions this node is replicated in.
  // Based on number of dimensions, insert loop instructions
  std::string varNames[3] = {"x", "y", "z"};
  unsigned numArgs = CI->getNumArgOperands();
  for (unsigned j = 0; j < C->getNumOfDim(); j++) {
    Value *indexLimit = NULL;
    // Limit can either be a constant or an arguement of the internal node.
    // In case of constant we can use that constant value directly in the
    // new F_CPU function. In case of an argument, we need to get the mapped
    // value using VMap
    if (isa<Constant>(C->getDimLimits()[j])) {
      indexLimit = C->getDimLimits()[j];
      DEBUG(errs() << "In Constant case:\n"
                   << "  indexLimit type = " << *indexLimit->getType() << "\n");
    } else {
      indexLimit = VMap[C->getDimLimits()[j]];
      DEBUG(errs() << "In VMap case:"
                   << "  indexLimit type = " << *indexLimit->getType() << "\n");
    }
    assert(indexLimit && "Invalid dimension limit!");
    // Insert loop
    Value *indexVar = addLoop(CI, indexLimit, varNames[j]);
    DEBUG(errs() << "indexVar type = " << *indexVar->getType() << "\n");
    // Insert index variable and limit arguments
    CI->setArgOperand(numArgs - 6 + j, indexVar);
    CI->setArgOperand(numArgs - 3 + j, indexLimit);
  }
  // Insert call to runtime to push the dim limits and instanceID on the depth
  // stack
  Value *args[] = {
      ConstantInt::get(Type::getInt32Ty(CI->getContext()),
                       C->getNumOfDim()), // numDim
      CI->getArgOperand(numArgs - 3 + 0), // limitX
      CI->getArgOperand(numArgs - 6 + 0), // iX
      CI->getArgOperand(numArgs - 3 + 1), // limitY
      CI->getArgOperand(numArgs - 6 + 1), // iY
      CI->getArgOperand(numArgs - 3 + 2), // limitZ
      CI->getArgOperand(numArgs - 6 + 2)  // iZ
  };

  CallInst *Push = CallInst::Create(llvm_hpvm_cpu_dstack_push,
                                    ArrayRef<Value *>(args, 7), "", CI);
  DEBUG(errs() << "Push on stack: " << *Push << "\n");
  // Insert call to runtime to pop the dim limits and instanceID from the depth
  // stack
  BasicBlock::iterator i(CI);
  ++i;
  Instruction *NextI = &*i;
  // Next Instruction should also belong to the same basic block as the basic
  // block will have a terminator instruction
  assert(NextI->getParent() == CI->getParent() &&
         "Next Instruction should also belong to the same basic block!");

  CallInst *Pop = CallInst::Create(llvm_hpvm_cpu_dstack_pop, None, "", NextI);
  DEBUG(errs() << "Pop from stack: " << *Pop << "\n");
  DEBUG(errs() << *CI->getParent()->getParent());
}

/* This function takes a DFNode, and creates a filter function for it. By filter
 * function we mean a function which keeps on getting input from input buffers,
 * applying the function on the inputs and then pushes data on output buffers
 */
// Create a function with void* (void*) type.
// Create a new basic block
// Add a return instruction to the basic block
// extract arguments from the aggregate data input. Type list would be
// Replace the streaming inputs with i8* types signifying handle to
// corresponding buffers
// Add a boolean argument isLastInput
// Add runtime API calls to get input for each of the streaming inputs
// Add a call to the generated function of the child node
// Add runtime API calls to push output for each of the streaming outputs
// Add loop around the basic block, which exits the loop if isLastInput is false

Function *CGT_CPU::createFunctionFilter(DFNode *C) {
  DEBUG(errs() << "*********Creating Function filter for "
               << C->getFuncPointer()->getName() << "*****\n");

  /* Create a function with same argument list as child.*/
  DEBUG(errs() << "\tCreate a function with the same argument list as child\n");
  // Get the generated function for child node
  Function *CF = C->getFuncPointer();
  // Create Filter Function of type i8*(i8*) which calls the root function
  Type *i8Ty = Type::getInt8Ty(M.getContext());
  FunctionType *CF_PipelineTy = FunctionType::get(
      i8Ty->getPointerTo(), ArrayRef<Type *>(i8Ty->getPointerTo()), false);
  Function *CF_Pipeline = Function::Create(CF_PipelineTy, CF->getLinkage(),
                                           CF->getName() + "_Pipeline", &M);
  DEBUG(errs() << "Generating Pipeline Function\n");
  // Give a name to the argument which is used pass data to this thread
  Value *data = &*CF_Pipeline->arg_begin();
  data->setName("data.addr");
  // Create a new basic block
  DEBUG(errs() << "\tCreate new BB and add a return function\n");
  // Add a basic block to this empty function
  BasicBlock *BB =
      BasicBlock::Create(CF_Pipeline->getContext(), "entry", CF_Pipeline);
  // Add a return instruction to the basic block
  ReturnInst *RI =
      ReturnInst::Create(CF_Pipeline->getContext(),
                         UndefValue::get(CF_Pipeline->getReturnType()), BB);

  /* Extract the elements from the aggregate argument to the function.
   * Replace the streaming inputs with i8* types signifying handle to
   * corresponding buffers
   * Add outputs to the list as well
   * Add isLastInput to the list
   */
  DEBUG(errs() << "\tReplace streaming input arguments with i8* type\n");
  // These Args will be used when passing arguments to the generated function
  // inside loop, and reading outputs as well.
  std::vector<Value *> Args;
  std::vector<Type *> TyList;
  std::vector<std::string> names;
  // Adding inputs
  for (Function::arg_iterator i = CF->arg_begin(), e = CF->arg_end(); i != e;
       ++i) {
    if (C->getInDFEdgeAt(i->getArgNo())->isStreamingEdge()) {
      TyList.push_back(i8Ty->getPointerTo());
      names.push_back((Twine(i->getName()) + "_buffer").str());
    } else {
      TyList.push_back(i->getType());
      names.push_back(i->getName());
    }
  }
  // Adding outputs. FIXME: Since we assume all outputs to be streaming edges,
  // because we get there buffer handles
  StructType *RetTy = cast<StructType>(CF->getReturnType());
  for (unsigned i = 0; i < RetTy->getNumElements(); i++) {
    TyList.push_back(i8Ty->getPointerTo());
    names.push_back("out");
  }
  /* Add a boolean argument isLastInput */
  DEBUG(errs() << "\tAdd a boolean argument called isLastInput to function\n");
  TyList.push_back(i8Ty->getPointerTo());
  names.push_back("isLastInput_buffer");

  // Extract the inputs, outputs
  Args = extractElements(data, TyList, names, RI);
  for (unsigned i = 0; i < Args.size(); i++) {
    DEBUG(errs() << *Args[i] << "\n");
  }

  // Split the Args vector into, input output and isLastInput
  unsigned numInputs = CF->getFunctionType()->getNumParams();
  unsigned numOutputs = RetTy->getNumElements();
  std::vector<Value *> InputArgs(Args.begin(), Args.begin() + numInputs);
  std::vector<Value *> OutputArgs(Args.begin() + numInputs,
                                  Args.begin() + numInputs + numOutputs);
  Instruction *isLastInput = cast<Instruction>(Args[Args.size() - 1]);

  /* Add runtime API calls to get input for each of the streaming input edges */
  DEBUG(errs() << "\tAdd runtime API calls to get input for each of the "
                  "streaming input edges\n");
  // First read the termination condition variable islastInput
  CallInst *isLastInputPop = CallInst::Create(
      llvm_hpvm_bufferPop, ArrayRef<Value *>(isLastInput), "", RI);

  CastInst *BI = BitCastInst::CreateIntegerCast(
      isLastInputPop, Type::getInt64Ty(CF_Pipeline->getContext()), false,
      "isLastInput", RI);
  isLastInput = BI;
  // Create a loop termination condition
  CmpInst *Cond = CmpInst::Create(
      Instruction::ICmp, CmpInst::ICMP_NE, isLastInput,
      Constant::getNullValue(Type::getInt64Ty(CF->getContext())),
      "isLastInputNotZero", RI);

  // Get input from buffers of all the incoming streaming edges
  for (Function::arg_iterator i = CF->arg_begin(), e = CF->arg_end(); i != e;
       ++i) {
    if (C->getInDFEdgeAt(i->getArgNo())->isStreamingEdge()) {
      CallInst *bufferIn =
          CallInst::Create(llvm_hpvm_bufferPop,
                           ArrayRef<Value *>(InputArgs[i->getArgNo()]), "", RI);
      CastInst *BI;
      if (i->getType()->isPointerTy()) {
        BI = CastInst::Create(CastInst::IntToPtr, bufferIn, i->getType(),
                              i->getName() + ".addr", RI);
      } else if (i->getType()->isFloatTy()) {
        BI = CastInst::CreateFPCast(bufferIn, i->getType(),
                                    i->getName() + ".addr", RI);
      } else {
        BI = CastInst::CreateIntegerCast(bufferIn, i->getType(), false,
                                         i->getName() + ".addr", RI);
      }
      // Replace the argument in Args vector. We would be using the vector as
      // parameters passed to the call
      InputArgs[i->getArgNo()] = BI;
    }
  }
  /* Add a call to the generated function of the child node */
  DEBUG(errs() << "\tAdd a call to the generated function of the child node\n");
  //  DEBUG(errs() << "Type: " << *C->getGenFunc()->getType() << "\n");
  //  CallInst* CI = CallInst::Create(C->getGenFunc(), InputArgs,
  //                                  C->getGenFunc()->getName()+".output", RI);
  Function *CGenF = C->getGenFuncForTarget(hpvm::CPU_TARGET);
  DEBUG(errs() << "Type: " << *CGenF->getType() << "\n");
  CallInst *CI =
      CallInst::Create(CGenF, InputArgs, CGenF->getName() + ".output", RI);

  /* Add runtime API calls to push output for each of the streaming outputs */
  // FIXME: Assumption
  // All edges between siblings are streaming edges
  DEBUG(errs() << "\tAdd runtime API calls to push output for each of the "
                  "streaming outputs\n");
  for (unsigned i = 0; i < numOutputs; i++) {
    // Extract output
    ExtractValueInst *EI =
        ExtractValueInst::Create(CI, ArrayRef<unsigned>(i), "", RI);
    // Convert to i64
    CastInst *BI;
    if (EI->getType()->isPointerTy())
      BI =
          CastInst::Create(CastInst::PtrToInt, EI,
                           Type::getInt64Ty(CF_Pipeline->getContext()), "", RI);
    else
      BI = CastInst::CreateIntegerCast(
          EI, Type::getInt64Ty(CF_Pipeline->getContext()), false, "", RI);
    // Push to Output buffer
    Value *bufferOutArgs[] = {OutputArgs[i], BI};
    CallInst::Create(llvm_hpvm_bufferPush, ArrayRef<Value *>(bufferOutArgs, 2),
                     "", RI);
  }

  // Add loop around the basic block, which exits the loop if isLastInput is
  // false Pointers to keep the created loop structure
  Instruction *CondStartI = cast<Instruction>(isLastInputPop);
  Instruction *BodyStartI = cast<Instruction>(Cond)->getNextNode();
  addWhileLoop(CondStartI, BodyStartI, RI, Cond);

  // Return the Function pointer
  DEBUG(errs() << "Pipeline Version of " << CF->getName() << ":\n");
  DEBUG(errs() << *CF_Pipeline << "\n");
  return CF_Pipeline;
}

void CGT_CPU::codeGen(DFInternalNode *N) {
  // Check if N is root node and its graph is streaming. We do not do codeGen
  // for Root in such a case
  if (N->isRoot() && N->isChildGraphStreaming())
    return;

  // Check if clone already exists. If it does, it means we have visited this
  // function before and nothing else needs to be done for this leaf node.
  //  if(N->getGenFunc() != NULL)
  //    return;
  if (!preferredTargetIncludes(N, hpvm::CPU_TARGET)) {
    DEBUG(errs() << "No CPU hint for node " << N->getFuncPointer()->getName()
                 << " : skipping it\n");
    return;
  }

  assert(N->getGenFuncForTarget(hpvm::CPU_TARGET) == NULL &&
         "Error: Visiting a node for which code already generated\n");

  // Sort children in topological order before code generation
  N->getChildGraph()->sortChildren();

  // Only process if all children have a CPU cpu function
  // Otherwise skip to end
  bool codeGen = true;
  for (DFGraph::children_iterator ci = N->getChildGraph()->begin(),
                                  ce = N->getChildGraph()->end();
       ci != ce; ++ci) {
    DFNode *C = *ci;
    // Skip dummy node call
    if (C->isDummyNode())
      continue;

    if (!(C->hasCPUGenFuncForTarget(hpvm::CPU_TARGET))) {
      DEBUG(errs() << "No CPU cpu version for child node "
                   << C->getFuncPointer()->getName()
                   << "\n  Skip code gen for parent node "
                   << N->getFuncPointer()->getName() << "\n");
      codeGen = false;
    }
  }

  if (codeGen) {
    Function *F = N->getFuncPointer();
    // Create of clone of F with no instructions. Only the type is the same as F
    // without the extra arguments.
    Function *F_CPU;

    // Clone the function, if we are seeing this function for the first time. We
    // only need a clone in terms of type.
    ValueToValueMapTy VMap;

    // Create new function with the same type
    F_CPU = Function::Create(F->getFunctionType(), F->getLinkage(),
                             F->getName(), &M);

    // Loop over the arguments, copying the names of arguments over.
    Function::arg_iterator dest_iterator = F_CPU->arg_begin();
    for (Function::const_arg_iterator i = F->arg_begin(), e = F->arg_end();
         i != e; ++i) {
      dest_iterator->setName(i->getName()); // Copy the name over...
      // Increment dest iterator
      ++dest_iterator;
    }

    // Add a basic block to this empty function
    BasicBlock *BB = BasicBlock::Create(F_CPU->getContext(), "entry", F_CPU);
    ReturnInst *RI = ReturnInst::Create(
        F_CPU->getContext(), UndefValue::get(F_CPU->getReturnType()), BB);

    // Add Index and Dim arguments except for the root node and the child graph
    // of parent node is not streaming
    if (!N->isRoot() && !N->getParent()->isChildGraphStreaming())
      F_CPU = addIdxDimArgs(F_CPU);

    BB = &*F_CPU->begin();
    RI = cast<ReturnInst>(BB->getTerminator());

    // Add generated function info to DFNode
    //    N->setGenFunc(F_CPU, hpvm::CPU_TARGET);
    N->addGenFunc(F_CPU, hpvm::CPU_TARGET, true);

    // Loop over the arguments, to create the VMap.
    dest_iterator = F_CPU->arg_begin();
    for (Function::const_arg_iterator i = F->arg_begin(), e = F->arg_end();
         i != e; ++i) {
      // Add mapping and increment dest iterator
      VMap[&*i] = &*dest_iterator;
      ++dest_iterator;
    }

    // Iterate over children in topological order
    for (DFGraph::children_iterator ci = N->getChildGraph()->begin(),
                                    ce = N->getChildGraph()->end();
         ci != ce; ++ci) {
      DFNode *C = *ci;
      // Skip dummy node call
      if (C->isDummyNode())
        continue;

      // Create calls to CPU function of child node
      invokeChild_CPU(C, F_CPU, VMap, RI);
    }

    DEBUG(errs() << "*** Generating epilogue code for the function****\n");
    // Generate code for output bindings
    // Get Exit node
    DFNode *C = N->getChildGraph()->getExit();
    // Get OutputType of this node
    StructType *OutTy = N->getOutputType();
    Value *retVal = UndefValue::get(F_CPU->getReturnType());
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
        assert(OutputMap.count(SrcDF) &&
               "Source node call not found. Dependency violation!");

        // Find Output Value associated with the Source DFNode using OutputMap
        Value *CI = OutputMap[SrcDF];

        // Extract element at source position from this call instruction
        std::vector<unsigned> IndexList;
        IndexList.push_back(E->getSourcePosition());
        DEBUG(errs() << "Going to generate ExtarctVal inst from " << *CI
                     << "\n");
        ExtractValueInst *EI = ExtractValueInst::Create(CI, IndexList, "", RI);
        inputVal = EI;
      }
      std::vector<unsigned> IdxList;
      IdxList.push_back(i);
      retVal = InsertValueInst::Create(retVal, inputVal, IdxList, "", RI);
    }
    DEBUG(errs() << "Extracted all\n");
    retVal->setName("output");
    ReturnInst *newRI = ReturnInst::Create(F_CPU->getContext(), retVal);
    ReplaceInstWithInst(RI, newRI);
  }

  //-------------------------------------------------------------------------//
  // Here, we need to check if this node (N) has more than one versions
  // If so, we query the policy and have a call to each version
  // If not, we see which version exists, check that it is in fact an cpu
  // function and save it as the CPU_TARGET function

  // TODO: hpvm_id per node, so we can use this for id for policies
  // For now, use node function name and change it later
  Function *CF = N->getGenFuncForTarget(hpvm::CPU_TARGET);
  Function *GF = N->getGenFuncForTarget(hpvm::GPU_TARGET);

  bool CFcpu = N->hasCPUGenFuncForTarget(hpvm::CPU_TARGET);
  bool GFcpu = N->hasCPUGenFuncForTarget(hpvm::GPU_TARGET);

  DEBUG(errs() << "Before editing\n");
  DEBUG(errs() << "Node: " << N->getFuncPointer()->getName() << " with tag "
               << N->getTag() << "\n");
  DEBUG(errs() << "CPU Fun: " << (CF ? CF->getName() : "null") << "\n");
  DEBUG(errs() << "hascpuGenFuncForCPU : " << CFcpu << "\n");
  DEBUG(errs() << "GPU Fun: " << (GF ? GF->getName() : "null") << "\n");
  DEBUG(errs() << "hascpuGenFuncForGPU : " << GFcpu << "\n");

  if (N->getTag() == hpvm::None) {
    // No code is available for this node. This (usually) means that this
    // node is a node that
    // - from the accelerator backends has been mapped to an intermediate
    // node, and thus they have not produced a genFunc
    // - a child node had no CPU hint, thus no code gen for CPU could
    // take place
    DEBUG(errs() << "No GenFunc - Skipping CPU code generation for node "
                 << N->getFuncPointer()->getName() << "\n");
  } else if (hpvmUtils::isSingleTargetTag(N->getTag())) {
    // There is a single version for this node according to code gen hints.
    // Therefore, we do not need to check the policy, we simply use the
    // available implementation, whichever target it is for.

    // Sanity check - to be removed TODO
    switch (N->getTag()) {
    case hpvm::CPU_TARGET:
      assert(N->getGenFuncForTarget(hpvm::CPU_TARGET) && "");
      assert(N->hasCPUGenFuncForTarget(hpvm::CPU_TARGET) && "");
      assert(!(N->getGenFuncForTarget(hpvm::GPU_TARGET)) && "");
      assert(!(N->hasCPUGenFuncForTarget(hpvm::GPU_TARGET)) && "");
      break;
    case hpvm::GPU_TARGET:
      assert(!(N->getGenFuncForTarget(hpvm::CPU_TARGET)) && "");
      assert(!(N->hasCPUGenFuncForTarget(hpvm::CPU_TARGET)) && "");
      assert(N->getGenFuncForTarget(hpvm::GPU_TARGET) && "");
      assert(N->hasCPUGenFuncForTarget(hpvm::GPU_TARGET) && "");
      break;
    default:
      assert(false && "Unreachable: we checked that tag was single target!\n");
      break;
    }

    N->addGenFunc(N->getGenFuncForTarget(N->getTag()), hpvm::CPU_TARGET, true);
    N->removeGenFuncForTarget(hpvm::GPU_TARGET);
    N->setTag(hpvm::CPU_TARGET);

    // Sanity checks - to be removed TODO
    CF = N->getGenFuncForTarget(hpvm::CPU_TARGET);
    GF = N->getGenFuncForTarget(hpvm::GPU_TARGET);

    CFcpu = N->hasCPUGenFuncForTarget(hpvm::CPU_TARGET);
    GFcpu = N->hasCPUGenFuncForTarget(hpvm::GPU_TARGET);

    DEBUG(errs() << "After editing\n");
    DEBUG(errs() << "Node: " << N->getFuncPointer()->getName() << " with tag "
                 << N->getTag() << "\n");
    DEBUG(errs() << "CPU Fun: " << (CF ? CF->getName() : "null") << "\n");
    DEBUG(errs() << "hascpuGenFuncForCPU : " << CFcpu << "\n");
    DEBUG(errs() << "GPU Fun: " << (GF ? GF->getName() : "null") << "\n");
    DEBUG(errs() << "hascpuGenFuncForGPU : " << GFcpu << "\n");

  } else {
    assert(false && "Multiple tags unsupported!");
  }
}

// Code generation for leaf nodes
void CGT_CPU::codeGen(DFLeafNode *N) {
  // Skip code generation if it is a dummy node
  if (N->isDummyNode()) {
    DEBUG(errs() << "Skipping dummy node\n");
    return;
  }

  // At this point, the CPU backend does not support code generation for
  // the case where allocation node is used, so we skip. This means that a
  // CPU version will not be created, and therefore code generation will
  // only succeed if another backend (opencl or spir) has been invoked to
  // generate a node function for the node including the allocation node.
  if (N->isAllocationNode()) {
    DEBUG(errs() << "Skipping allocation node\n");
    return;
  }

  // Check if clone already exists. If it does, it means we have visited this
  // function before and nothing else needs to be done for this leaf node.
  //  if(N->getGenFunc() != NULL)
  //    return;

  if (!preferredTargetIncludes(N, hpvm::CPU_TARGET)) {
    DEBUG(errs() << "No CPU hint for node " << N->getFuncPointer()->getName()
                 << " : skipping it\n");

    switch (N->getTag()) {
    case hpvm::GPU_TARGET:
      // A leaf node should not have an cpu function for GPU
      // by design of DFG2LLVM_OpenCL backend
      assert(!(N->hasCPUGenFuncForTarget(hpvm::GPU_TARGET)) &&
             "Leaf node not expected to have GPU GenFunc");
      break;
    default:
      break;
    }

    return;
  }

  assert(N->getGenFuncForTarget(hpvm::CPU_TARGET) == NULL &&
         "Error: Visiting a node for which code already generated\n");

  std::vector<IntrinsicInst *> IItoRemove;
  std::vector<std::pair<IntrinsicInst *, Value *>> IItoReplace;
  BuildDFG::HandleToDFNode Leaf_HandleToDFNodeMap;

  // Get the function associated woth the dataflow node
  Function *F = N->getFuncPointer();

  // Clone the function, if we are seeing this function for the first time.
  Function *F_CPU;
  ValueToValueMapTy VMap;
  F_CPU = CloneFunction(F, VMap);
  F_CPU->removeFromParent();
  // Insert the cloned function into the module
  M.getFunctionList().push_back(F_CPU);

  // Add the new argument to the argument list. Add arguments only if the cild
  // graph of parent node is not streaming
  if (!N->getParent()->isChildGraphStreaming())
    F_CPU = addIdxDimArgs(F_CPU);

  // Add generated function info to DFNode
  //  N->setGenFunc(F_CPU, hpvm::CPU_TARGET);
  N->addGenFunc(F_CPU, hpvm::CPU_TARGET, true);

  // Go through the arguments, and any pointer arguments with in attribute need
  // to have cpu_argument_ptr call to get the cpu ptr of the argument
  // Insert these calls in a new BB which would dominate all other BBs
  // Create new BB
  BasicBlock *EntryBB = &*F_CPU->begin();
  BasicBlock *BB =
      BasicBlock::Create(M.getContext(), "getHPVMPtrArgs", F_CPU, EntryBB);
  BranchInst *Terminator = BranchInst::Create(EntryBB, BB);
  // Insert calls
  for (Function::arg_iterator ai = F_CPU->arg_begin(), ae = F_CPU->arg_end();
       ai != ae; ++ai) {
    if (F_CPU->getAttributes().hasAttribute(ai->getArgNo() + 1,
                                            Attribute::In)) {
      assert(ai->getType()->isPointerTy() &&
             "Only pointer arguments can have hpvm in/out attributes ");
      Function::arg_iterator aiNext = ai;
      ++aiNext;
      Argument *size = &*aiNext;
      assert(size->getType() == Type::getInt64Ty(M.getContext()) &&
             "Next argument after a pointer should be an i64 type");
      CastInst *BI = BitCastInst::CreatePointerCast(
          &*ai, Type::getInt8PtrTy(M.getContext()), ai->getName() + ".i8ptr",
          Terminator);
      Value *ArgPtrCallArgs[] = {BI, size};
      CallInst::Create(llvm_hpvm_cpu_argument_ptr,
                       ArrayRef<Value *>(ArgPtrCallArgs, 2), "", Terminator);
    }
  }
  DEBUG(errs() << *BB << "\n");

  // Go through all the instructions
  for (inst_iterator i = inst_begin(F_CPU), e = inst_end(F_CPU); i != e; ++i) {
    Instruction *I = &(*i);
    DEBUG(errs() << *I << "\n");
    // Leaf nodes should not contain HPVM graph intrinsics or launch
    assert(!BuildDFG::isHPVMLaunchIntrinsic(I) &&
           "Launch intrinsic within a dataflow graph!");
    assert(!BuildDFG::isHPVMGraphIntrinsic(I) &&
           "HPVM graph intrinsic within a leaf dataflow node!");

    if (BuildDFG::isHPVMQueryIntrinsic(I)) {
      IntrinsicInst *II = cast<IntrinsicInst>(I);
      IntrinsicInst *ArgII;
      DFNode *ArgDFNode;

      /***********************************************************************
       *                        Handle HPVM Query intrinsics                  *
       ***********************************************************************/
      switch (II->getIntrinsicID()) {
      /**************************** llvm.hpvm.getNode() *******************/
      case Intrinsic::hpvm_getNode: {
        // add mapping <intrinsic, this node> to the node-specific map
        Leaf_HandleToDFNodeMap[II] = N;
        IItoRemove.push_back(II);
        break;
      }
      /************************* llvm.hpvm.getParentNode() ****************/
      case Intrinsic::hpvm_getParentNode: {
        // get the parent node of the arg node
        // get argument node
        ArgII = cast<IntrinsicInst>((II->getOperand(0))->stripPointerCasts());
        // get the parent node of the arg node
        ArgDFNode = Leaf_HandleToDFNodeMap[ArgII];
        // Add mapping <intrinsic, parent node> to the node-specific map
        // the argument node must have been added to the map, orelse the
        // code could not refer to it
        Leaf_HandleToDFNodeMap[II] = ArgDFNode->getParent();
        IItoRemove.push_back(II);
        break;
      }
      /*************************** llvm.hpvm.getNumDims() *****************/
      case Intrinsic::hpvm_getNumDims: {
        // get node from map
        // get the appropriate field
        ArgII = cast<IntrinsicInst>((II->getOperand(0))->stripPointerCasts());
        int numOfDim = Leaf_HandleToDFNodeMap[ArgII]->getNumOfDim();
        IntegerType *IntTy = Type::getInt32Ty(M.getContext());
        ConstantInt *numOfDimConstant =
            ConstantInt::getSigned(IntTy, (int64_t)numOfDim);

        II->replaceAllUsesWith(numOfDimConstant);
        IItoRemove.push_back(II);
        break;
      }
      /*********************** llvm.hpvm.getNodeInstanceID() **************/
      case Intrinsic::hpvm_getNodeInstanceID_x:
      case Intrinsic::hpvm_getNodeInstanceID_y:
      case Intrinsic::hpvm_getNodeInstanceID_z: {
        ArgII = cast<IntrinsicInst>((II->getOperand(0))->stripPointerCasts());
        ArgDFNode = Leaf_HandleToDFNodeMap[ArgII];

        // The dfnode argument should be an ancestor of this leaf node or
        // the leaf node itself
        int parentLevel = N->getAncestorHops(ArgDFNode);
        assert((parentLevel >= 0 || ArgDFNode == (DFNode *)N) &&
               "Invalid DFNode argument to getNodeInstanceID_[xyz]!");

        // Get specified dimension
        // (dim = 0) => x
        // (dim = 1) => y
        // (dim = 2) => z
        int dim =
            (int)(II->getIntrinsicID() - Intrinsic::hpvm_getNodeInstanceID_x);
        assert((dim >= 0) && (dim < 3) &&
               "Invalid dimension for getNodeInstanceID_[xyz]. Check Intrinsic "
               "ID!");

        // For immediate ancestor, use the extra argument introduced in
        // F_CPU
        int numParamsF = F->getFunctionType()->getNumParams();
        int numParamsF_CPU = F_CPU->getFunctionType()->getNumParams();
        assert(
            (numParamsF_CPU - numParamsF == 6) &&
            "Difference of arguments between function and its clone is not 6!");

        if (parentLevel == 0) {
          // Case when the query is for this node itself
          unsigned offset = 3 + (3 - dim);
          // Traverse argument list of F_CPU in reverse order to find the
          // correct index or dim argument.
          Argument *indexVal = getArgumentFromEnd(F_CPU, offset);
          assert(indexVal && "Index argument not found. Invalid offset!");

          DEBUG(errs() << *II << " replaced with " << *indexVal << "\n");

          II->replaceAllUsesWith(indexVal);
          IItoRemove.push_back(II);
        } else {
          // Case when query is for an ancestor
          Value *args[] = {
              ConstantInt::get(Type::getInt32Ty(II->getContext()), parentLevel),
              ConstantInt::get(Type::getInt32Ty(II->getContext()), dim)};
          CallInst *CI = CallInst::Create(llvm_hpvm_cpu_getDimInstance,
                                          ArrayRef<Value *>(args, 2),
                                          "nodeInstanceID", II);
          DEBUG(errs() << *II << " replaced with " << *CI << "\n");
          II->replaceAllUsesWith(CI);
          IItoRemove.push_back(II);
        }
        break;
      }
      /********************** llvm.hpvm.getNumNodeInstances() *************/
      case Intrinsic::hpvm_getNumNodeInstances_x:
      case Intrinsic::hpvm_getNumNodeInstances_y:
      case Intrinsic::hpvm_getNumNodeInstances_z: {

        ArgII = cast<IntrinsicInst>((II->getOperand(0))->stripPointerCasts());
        ArgDFNode = Leaf_HandleToDFNodeMap[ArgII];

        // The dfnode argument should be an ancestor of this leaf node or
        // the leaf node itself
        int parentLevel = N->getAncestorHops(ArgDFNode);
        assert((parentLevel >= 0 || ArgDFNode == (DFNode *)N) &&
               "Invalid DFNode argument to getNodeInstanceID_[xyz]!");

        // Get specified dimension
        // (dim = 0) => x
        // (dim = 1) => y
        // (dim = 2) => z
        int dim =
            (int)(II->getIntrinsicID() - Intrinsic::hpvm_getNumNodeInstances_x);
        assert((dim >= 0) && (dim < 3) &&
               "Invalid dimension for getNumNodeInstances_[xyz]. Check "
               "Intrinsic ID!");

        // For immediate ancestor, use the extra argument introduced in
        // F_CPU
        int numParamsF = F->getFunctionType()->getNumParams();
        int numParamsF_CPU = F_CPU->getFunctionType()->getNumParams();
        assert(
            (numParamsF_CPU - numParamsF == 6) &&
            "Difference of arguments between function and its clone is not 6!");

        if (parentLevel == 0) {
          // Case when the query is for this node itself
          unsigned offset = 3 - dim;
          // Traverse argument list of F_CPU in reverse order to find the
          // correct index or dim argument.
          Argument *limitVal = getArgumentFromEnd(F_CPU, offset);
          assert(limitVal && "Limit argument not found. Invalid offset!");

          DEBUG(errs() << *II << " replaced with " << *limitVal << "\n");

          II->replaceAllUsesWith(limitVal);
          IItoRemove.push_back(II);
        } else {
          // Case when query is from the ancestor
          Value *args[] = {
              ConstantInt::get(Type::getInt32Ty(II->getContext()), parentLevel),
              ConstantInt::get(Type::getInt32Ty(II->getContext()), dim)};
          CallInst *CI = CallInst::Create(llvm_hpvm_cpu_getDimLimit,
                                          ArrayRef<Value *>(args, 2),
                                          "numNodeInstances", II);
          DEBUG(errs() << *II << " replaced with " << *CI << "\n");
          II->replaceAllUsesWith(CI);
          IItoRemove.push_back(II);
        }

        break;
      }
      default:
        DEBUG(errs() << "Found unknown intrinsic with ID = "
                     << II->getIntrinsicID() << "\n");
        assert(false && "Unknown HPVM Intrinsic!");
        break;
      }

    } else {
    }
  }

  // Remove them in reverse order
  for (std::vector<IntrinsicInst *>::iterator i = IItoRemove.begin();
       i != IItoRemove.end(); ++i) {
    (*i)->replaceAllUsesWith(UndefValue::get((*i)->getType()));
    (*i)->eraseFromParent();
  }

  DEBUG(errs() << *F_CPU);
}

} // End of namespace

char DFG2LLVM_CPU::ID = 0;
static RegisterPass<DFG2LLVM_CPU>
    X("dfg2llvm-cpu", "Dataflow Graph to LLVM for CPU backend",
      false /* does not modify the CFG */,
      true /* transformation, not just analysis */);
