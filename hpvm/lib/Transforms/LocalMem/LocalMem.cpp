//===-------------------------- LocalMem.cpp --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass traverses the dataflow graph to recognize the allocation nodes
// which allocate scratch memory. This pass does not make changes to the textual
// representation of HPVM IR.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "LocalMem"
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

using namespace llvm;
using namespace builddfg;
using namespace dfg2llvm;

namespace {
// Helper Functions

static AllocationNodeProperty *isAllocationNode(DFLeafNode *N);

// LocalMem - The first implementation.
struct LocalMem : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  LocalMem() : ModulePass(ID) {}

private:
  // Member variables

  // Functions

public:
  bool runOnModule(Module &M);

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<BuildDFG>();
    AU.addPreserved<BuildDFG>();
  }
};

// Visitor for Code generation traversal (tree traversal for now)
class AT_OCL : public CodeGenTraversal {

private:
  // Member variables

  // Functions

  // Virtual Functions
  void init() {}
  void initRuntimeAPI() {}
  void codeGen(DFInternalNode *N);
  void codeGen(DFLeafNode *N);

public:
  // Constructor
  AT_OCL(Module &_M, BuildDFG &_DFG) : CodeGenTraversal(_M, _DFG) {
    // init();
    // initRuntimeAPI();
  }
};

bool LocalMem::runOnModule(Module &M) {
  DEBUG(errs() << "\nLOCALMEM PASS\n");

  // Get the BuildDFG Analysis Results:
  // - Dataflow graph
  // - Maps from i8* hansles to DFNode and DFEdge
  BuildDFG &DFG = getAnalysis<BuildDFG>();

  // DFInternalNode *Root = DFG.getRoot();
  std::vector<DFInternalNode *> Roots = DFG.getRoots();
  // BuildDFG::HandleToDFNode &HandleToDFNodeMap = DFG.getHandleToDFNodeMap();
  // BuildDFG::HandleToDFEdge &HandleToDFEdgeMap = DFG.getHandleToDFEdgeMap();

  // Visitor for Code Generation Graph Traversal
  AT_OCL *ATVisitor = new AT_OCL(M, DFG);

  // Iterate over all the DFGs and produce code for each one of them
  for (auto rootNode : Roots) {
    // Initiate code generation for root DFNode
    ATVisitor->visit(rootNode);
    // Go ahead and replace the launch intrinsic with pthread call, otherwise
    // return now.
    // TODO: Later on, we might like to do this in a separate pass, which would
    // allow us the flexibility to switch between complete static code
    // generation for DFG or having a customized runtime+scheduler
  }

  delete ATVisitor;
  return true;
}

void AT_OCL::codeGen(DFInternalNode *N) {
  DEBUG(errs() << "Analysing Node: " << N->getFuncPointer()->getName() << "\n");
}

// Code generation for leaf nodes
void AT_OCL::codeGen(DFLeafNode *N) {
  DEBUG(errs() << "Analysing Node: " << N->getFuncPointer()->getName() << "\n");
  // Skip code generation if it is a dummy node
  if (N->isDummyNode()) {
    DEBUG(errs() << "Skipping dummy node\n");
    return;
  }

  // Check and mark as allocation node
  AllocationNodeProperty *ANP = isAllocationNode(N);
  if (ANP != NULL) {
    // set Properties of the allocation node
    N->setProperty(DFNode::Allocation, ANP);
    AllocationNodeProperty *anp =
        (AllocationNodeProperty *)N->getProperty(DFNode::Allocation);
    AllocationNodeProperty::AllocationListType AL = anp->getAllocationList();
    DEBUG(errs() << "Total allocations = " << AL.size() << "\n");
    for (auto P : AL) {
      DEBUG(errs() << " EdgePort: " << P.first->getDestPosition());
      DEBUG(errs() << " Size: " << *P.second << "\n");
    }
  }
}

// Return pointer to property if this leaf node matches the conditions for being
// an allocation node. Conditions
// 1. No incoming memory pointer. No in/out attribute on a pointer argument
// 2. Uses hpvm malloc intrinsic to allocate memory
// 3. Sends it out
// 2. (TODO:) Whether the allocated pointer escapes the parent node
AllocationNodeProperty *isAllocationNode(DFLeafNode *N) {
  // Allocation node must be free from side-effects
  if (N->hasSideEffects())
    return NULL;

  // Allocation node must have some outgoing edges
  if (N->getOutputType()->isEmptyTy())
    return NULL;

  Function *F = N->getFuncPointer();

  // Allocation node must use hpvm malloc intrinsic
  bool usesHPVMMalloc = false;
  for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; i++) {
    Instruction *I = &*i;
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(I)) {
      if (II->getIntrinsicID() == Intrinsic::hpvm_malloc) {
        usesHPVMMalloc = true;
        break;
      }
    }
  }
  if (!usesHPVMMalloc)
    return NULL;

  // TODO: Check if allocated pointer leaves parent node

  // This is an allocation node
  AllocationNodeProperty *ANP = new AllocationNodeProperty();
  // Find the return statement.
  // FIXME: For now, assuming their is just one BB. Terminator instruction of
  // this BB is a return statement. The value returned is what we need
  BasicBlock &BB = F->getEntryBlock();
  assert(isa<ReturnInst>(BB.getTerminator()) &&
         "Currently we do not handle the case where Allocation Node has "
         "multiple BB");
  ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator());
  // Find the returned struct
  Value *val = RI->getReturnValue();
  std::vector<Value *> OutValues(6, NULL);
  unsigned numOutputs = N->getOutputType()->getNumElements();
  for (unsigned i = 0; i < numOutputs; i++) {
    if (InsertValueInst *IV = dyn_cast<InsertValueInst>(val)) {
      DEBUG(errs() << "Value at out edge" << numOutputs - 1 - i << ": " << *val
                   << "\n");
      OutValues[numOutputs - 1 - i] = IV->getOperand(1);
      val = IV->getOperand(0);
    } else {
      DEBUG(errs() << "Unexpected value at out edge: " << *val << "\n");
      llvm_unreachable("Expecting InsertValue instruction. Error!");
    }
  }
  // OutValues vector contains all the values that will go out
  // Assume that the Allocation node only sends the pointers and their sizes
  // forward
  unsigned i = 0;
  while (i < numOutputs) {
    assert(OutValues[i]->getType()->isPointerTy() &&
           "Expected outgoing edge to be of pointer type");
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(OutValues[i])) {
      if (II->getIntrinsicID() == Intrinsic::hpvm_malloc) {
        // Sanity check: Size passed to malloc intrinsic is same as the value
        // going into the next outgoing edge
        DEBUG(errs() << "HPVM malloc size: " << *II->getArgOperand(0) << "\n");
        DEBUG(errs() << "Out edge value: " << *OutValues[i + 1] << "\n");
        assert(II->getArgOperand(0) == OutValues[i + 1] &&
               "Sanity Check Failed: HPVM Malloc size argument != next "
               "outgoing edge");
        ANP->insertAllocation(N->getOutDFEdgeAt(i), II->getArgOperand(0));
        i = i + 2;
        continue;
      }
    }
    llvm_unreachable("Expecting hpvm malloc intrinsic instruction!");
  }
  return ANP;
}

} // End of namespace

char LocalMem::ID = 0;
static RegisterPass<LocalMem>
    X("localmem",
      "Pass to identifying nodes amenable to local memory allocation",
      false /* does not modify the CFG */,
      true /* transformation, not just analysis */);
