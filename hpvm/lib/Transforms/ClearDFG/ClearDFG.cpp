//===-------------------------- ClearDFG.cpp --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass HPVM intrinsics from HPVM IR. This pass is the final pass that
// runs as a part of clean up after construction of dataflowgraph and LLVM
// code generation for different targets from the dataflow graph.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "ClearDFG"
#include "BuildDFG/BuildDFG.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

using namespace llvm;
using namespace builddfg;

// STATISTIC(IntrinsicCounter, "Counts number of hpvm intrinsics greeted");

namespace {

// ClearDFG - The first implementation.
struct ClearDFG : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  ClearDFG() : ModulePass(ID) {}

private:
  // Member variables

  // Functions

public:
  bool runOnModule(Module &M);

  void getAnalysisUsage(AnalysisUsage &AU) const { AU.addRequired<BuildDFG>(); }
};

// Visitor for Code generation traversal (tree traversal for now)
class TreeTraversal : public DFNodeVisitor {

private:
  // Member variables
  Module &M;
  BuildDFG &DFG;

  // Map from Old function associated with DFNode to new cloned function with
  // extra index and dimension arguments. This map also serves to find out if
  // we already have an index and dim extended function copy or not (i.e.,
  // "Have we visited this function before?")
  ValueMap<Function *, Function *> FMap;
  DenseMap<DFNode *, CallInst *> CallMap;

  // Functions
  void deleteNode(DFNode *N);

public:
  // Constructor
  TreeTraversal(Module &_M, BuildDFG &_DFG) : M(_M), DFG(_DFG) {}

  virtual void visit(DFInternalNode *N) {
    // Follows a bottom-up approach for code generation.
    // First generate code for all the child nodes
    for (DFGraph::children_iterator i = N->getChildGraph()->begin(),
                                    e = N->getChildGraph()->end();
         i != e; ++i) {
      DFNode *child = *i;
      child->applyDFNodeVisitor(*this);
    }
    DEBUG(errs() << "Erasing Node (I) - " << N->getFuncPointer()->getName()
                 << "\n");
    // Generate code for this internal node now. This way all the cloned
    // functions for children exist.
    deleteNode(N);
    DEBUG(errs() << "\tDone - "
                 << "\n");
    // errs() << "DONE: Generating Code for Node (I) - " <<
    // N->getFuncPointer()->getName() << "\n";
  }

  virtual void visit(DFLeafNode *N) {
    DEBUG(errs() << "Erasing Node (L) - " << N->getFuncPointer()->getName()
                 << "\n");
    deleteNode(N);
    DEBUG(errs() << "DONE"
                 << "\n");
  }
};

bool ClearDFG::runOnModule(Module &M) {
  DEBUG(errs() << "\nCLEARDFG PASS\n");
  // Get the BuildDFG Analysis Results:
  // - Dataflow graph
  // - Maps from i8* hansles to DFNode and DFEdge
  BuildDFG &DFG = getAnalysis<BuildDFG>();

  // DFInternalNode *Root = DFG.getRoot();
  std::vector<DFInternalNode *> Roots = DFG.getRoots();
  // BuildDFG::HandleToDFNode &HandleToDFNodeMap = DFG.getHandleToDFNodeMap();
  // BuildDFG::HandleToDFEdge &HandleToDFEdgeMap = DFG.getHandleToDFEdgeMap();

  Function *VI = M.getFunction("llvm.hpvm.init");
  assert(VI->hasOneUse() && "More than one use of llvm.hpvm.init\n");
  for (Value::user_iterator ui = VI->user_begin(), ue = VI->user_end();
       ui != ue; ui++) {
    Instruction *I = dyn_cast<Instruction>(*ui);
    I->eraseFromParent();
  }
  VI->replaceAllUsesWith(UndefValue::get(VI->getType()));
  VI->eraseFromParent();

  Function *VC = M.getFunction("llvm.hpvm.cleanup");
  assert(VC->hasOneUse() && "More than one use of llvm.hpvm.cleanup\n");
  for (Value::user_iterator ui = VC->user_begin(), ue = VC->user_end();
       ui != ue; ui++) {
    Instruction *I = dyn_cast<Instruction>(*ui);
    I->eraseFromParent();
  }

  VC->replaceAllUsesWith(UndefValue::get(VC->getType()));
  VC->eraseFromParent();

  // Visitor for Code Generation Graph Traversal
  TreeTraversal *Visitor = new TreeTraversal(M, DFG);

  // Initiate code generation for root DFNode
  for (auto rootNode : Roots) {
    Visitor->visit(rootNode);
  }
  delete Visitor;
  return true;
}

void TreeTraversal::deleteNode(DFNode *N) {
  if (N->isDummyNode())
    return;
  // Erase Function associated with this node
  Function *F = N->getFuncPointer();
  F->replaceAllUsesWith(UndefValue::get(F->getType()));
  F->eraseFromParent();
  // If N is not a root node, we are done. Return.
  if (!N->isRoot())
    return;
  // N is a root node. Delete the Launch Intrinsic associated it with as well.
  IntrinsicInst *LI = N->getInstruction();
  LI->replaceAllUsesWith(UndefValue::get(LI->getType()));
  LI->eraseFromParent();
}

} // End of namespace

char ClearDFG::ID = 0;
static RegisterPass<ClearDFG>
    X("clearDFG", "Delete all DFG functions for which code has been generated",
      false /* does not modify the CFG */,
      true /* transformation, not just analysis */);
