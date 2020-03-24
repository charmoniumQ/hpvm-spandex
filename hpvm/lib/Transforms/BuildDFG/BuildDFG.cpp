//=== BuildDFG.cpp - Implements "Hierarchical Dataflow Graph Builder Pass" ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// BuildDFG pass is responsible for constructing dataflow graph from a textual
// representation of HPVM IR with HPVM intrinsics from GenHPVM pass. This pass
// makes use of three crutial abstractions: graph itself, dataflow nodes repre-
// -senting functions and data edges representing tranfer of data between
// the functions (or nodes in the graph). This pass is part of HPVM frontend
// and does not make any changes to the textual representation of the IR.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "buildDFG"
#include "BuildDFG/BuildDFG.h"

#include "SupportHPVM/HPVMHint.h"
#include "SupportHPVM/HPVMUtils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace builddfg {

bool BuildDFG::runOnModule(Module &M) {
  DEBUG(errs() << "\nBUILDDFG PASS\n");
  DEBUG(errs() << "-------- Searching for launch sites ----------\n");

  IntrinsicInst *II;

  // Iterate over all functions in the module
  for (auto &Func : M) {
    Function *F = &Func;
    DEBUG(errs() << "Function: " << F->getName() << "\n");

    for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
      Instruction *I = &*i; // Grab pointer to Instruction
      if (isHPVMLaunchIntrinsic(I)) {
        DEBUG(errs() << "------------ Found launch site --------------\n");
        II = cast<IntrinsicInst>(I);

        assert(II && "Launch intrinsic not recognized.");

        // Intrinsic Instruction has been initialized from this point on.
        Function *F = cast<Function>(II->getOperand(0)->stripPointerCasts());
        Root = DFInternalNode::Create(II, F, hpvmUtils::getPreferredTarget(F));
        Roots.push_back(Root);
        BuildGraph(Root, F);

        for (DFGraph::children_iterator i = Root->getChildGraph()->begin(),
                                        e = Root->getChildGraph()->end();
             i != e; i++) {
          DFNode *N = *i;
          DEBUG(errs() << "\t" << N->getFuncPointer()->getName() << "\n");
        }
        Root->getChildGraph()->sortChildren();
        for (DFGraph::children_iterator i = Root->getChildGraph()->begin(),
                                        e = Root->getChildGraph()->end();
             i != e; i++) {
          DFNode *N = *i;
          DEBUG(errs() << "\t" << N->getFuncPointer()->getName() << "\n");
        }
        viewDFGraph(Root->getChildGraph());
      }
    }
  }

  // Checking that we found at least one launch site
  assert((Roots.size() != 0) && "Launch site not found.");

  return false;
}

DFInternalNode *BuildDFG::getRoot() const {
  assert(Root && Root->getLevel() == 0 && "Invalid root node.");
  return Root;
}

std::vector<DFInternalNode *> &BuildDFG::getRoots() {
  assert((Roots.size() != 0) && "Number of roots cannot be zero.");

  // All roots should have the same level
  for (auto *Node : Roots)
    assert(Node->getLevel() == 0 && "Invalid root node.");

  return Roots;
}

// TODO: Maybe make this const
BuildDFG::HandleToDFNode &BuildDFG::getHandleToDFNodeMap() {
  return HandleToDFNodeMap;
}

// TODO: Maybe make this const
BuildDFG::HandleToDFEdge &BuildDFG::getHandleToDFEdgeMap() {
  return HandleToDFEdgeMap;
}

void BuildDFG::addElementToHandleToDFNodeMap(Value *V, DFNode *N) {
  assert((HandleToDFNodeMap.find(V) == HandleToDFNodeMap.end()) &&
         "Attempted to insert duplicate key in HandleToDFNodeMap");
  HandleToDFNodeMap.insert(std::pair<Value *, DFNode *>(V, N));
}

// TODO: check if the removed element was not there
void BuildDFG::removeElementFromHandleToDFNodeMap(Value *V) {
  HandleToDFNodeMap.erase(V);
}

void BuildDFG::addElementToHandleToDFEdgeMap(Value *V, DFEdge *E) {
  assert((HandleToDFEdgeMap.find(V) == HandleToDFEdgeMap.end()) &&
         "Attempted to insert duplicate key in HandleToDFEdgeMap");
  HandleToDFEdgeMap.insert(std::pair<Value *, DFEdge *>(V, E));
}

// TODO: check if the removed element was not there
void BuildDFG::removeElementFromHandleToDFEdgeMap(Value *V) {
  HandleToDFEdgeMap.erase(V);
}

// Returns true if instruction I is a hpvm launch intrinsic, false otherwise
bool BuildDFG::isHPVMLaunchIntrinsic(Instruction *I) {
  if (!isa<IntrinsicInst>(I))
    return false;
  IntrinsicInst *II = cast<IntrinsicInst>(I);
  return (II->getCalledFunction()->getName()).equals("llvm.hpvm.launch");
}

// Returns true if instruction I is a hpvm graph intrinsic, false otherwise
bool BuildDFG::isHPVMGraphIntrinsic(Instruction *I) {
  if (!isa<IntrinsicInst>(I))
    return false;
  IntrinsicInst *II = cast<IntrinsicInst>(I);
  return (II->getCalledFunction()->getName()).startswith("llvm.hpvm.create") ||
         (II->getCalledFunction()->getName()).startswith("llvm.hpvm.bind");
}

// Returns true if instruction I is a hpvm query intrinsic, false otherwise
bool BuildDFG::isHPVMQueryIntrinsic(Instruction *I) {
  if (!isa<IntrinsicInst>(I))
    return false;
  IntrinsicInst *II = cast<IntrinsicInst>(I);
  return (II->getCalledFunction()->getName()).startswith("llvm.hpvm.get");
}

// Returns true if instruction I is a hpvm intrinsic, false otherwise
bool BuildDFG::isHPVMIntrinsic(Instruction *I) {
  if (!isa<IntrinsicInst>(I))
    return false;
  IntrinsicInst *II = cast<IntrinsicInst>(I);
  return (II->getCalledFunction()->getName()).startswith("llvm.hpvm");
}

// Two types are "congruent" if they are identical, or if they are both
// pointer types with different pointee types and the same address space.
bool BuildDFG::isTypeCongruent(Type *L, Type *R) {
  if (L == R)
    return true;
  PointerType *PL = dyn_cast<PointerType>(L);
  PointerType *PR = dyn_cast<PointerType>(R);
  if (!PL || !PR)
    return false;
  return PL->getAddressSpace() == PR->getAddressSpace();
}

// Handles all the createNodeXX hpvm intrinsics.
void BuildDFG::handleCreateNode(DFInternalNode *N, IntrinsicInst *II) {
  bool isInternalNode = false;

  Function *F = cast<Function>((II->getOperand(0))->stripPointerCasts());

  // Check if the function associated with this intrinsic is a leaf or
  // internal node
  for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
    Instruction *I = &*i; // Grab pointer to Instruction
    if (isHPVMGraphIntrinsic(I))
      isInternalNode = true;
  }

  // Number of Dimensions would be equal to the (number of operands - 1) as
  // the first operand is the pointer to associated Function and the
  // remaining operands are the limits in each dimension.
  unsigned numOfDim =
      II->getCalledFunction()->getFunctionType()->getNumParams() - 1;
  assert(numOfDim <= 3 &&
         "Invalid number of dimensions for createNode intrinsic!");
  std::vector<Value *> dimLimits;
  for (unsigned i = 1; i <= numOfDim; i++) {
    // The operands of II are same as the operands of the called
    // intrinsic. It has one extra operand at the end, which is the intrinsic
    // being called.
    dimLimits.push_back(cast<Value>(II->getOperand(i)));
  }

  if (isInternalNode) {
    // Create Internal DFNode, add it to the map and recursively build its
    // dataflow graph
    DFInternalNode *childDFNode = DFInternalNode::Create(
        II, F, hpvmUtils::getPreferredTarget(F), N, numOfDim, dimLimits);
    N->addChildToDFGraph(childDFNode);
    HandleToDFNodeMap[II] = childDFNode;
    BuildGraph(childDFNode, F);
  } else {
    // Create Leaf DFnode and add it to the map.
    DFLeafNode *childDFNode = DFLeafNode::Create(
        II, F, hpvmUtils::getPreferredTarget(F), N, numOfDim, dimLimits);
    N->addChildToDFGraph(childDFNode);
    HandleToDFNodeMap[II] = childDFNode;
  }
}

void BuildDFG::handleCreateEdge(DFInternalNode *N, IntrinsicInst *II) {
  // The DFNode structures must be in the map before the edge is processed
  HandleToDFNode::iterator DFI = HandleToDFNodeMap.find(II->getOperand(0));
  assert(DFI != HandleToDFNodeMap.end());
  DFI = HandleToDFNodeMap.find(II->getOperand(1));
  assert(DFI != HandleToDFNodeMap.end());

  DFNode *SrcDF = HandleToDFNodeMap[II->getOperand(0)];
  DFNode *DestDF = HandleToDFNodeMap[II->getOperand(1)];

  bool EdgeType = !cast<ConstantInt>(II->getOperand(2))->isZero();

  unsigned SourcePosition =
      cast<ConstantInt>(II->getOperand(3))->getZExtValue();
  unsigned DestPosition = cast<ConstantInt>(II->getOperand(4))->getZExtValue();

  bool isStreaming = !cast<ConstantInt>(II->getOperand(5))->isZero();

  Type *SrcTy, *DestTy;

  // Get destination type
  FunctionType *FT = DestDF->getFuncPointer()->getFunctionType();
  assert((FT->getNumParams() > DestPosition) &&
         "Invalid argument number for destination dataflow node!");
  DestTy = FT->getParamType(DestPosition);

  // Get source type
  StructType *OutTy = SrcDF->getOutputType();
  assert((OutTy->getNumElements() > SourcePosition) &&
         "Invalid argument number for source dataflow node!");
  SrcTy = OutTy->getElementType(SourcePosition);

  // check if the types are compatible
  assert(isTypeCongruent(SrcTy, DestTy) &&
         "Source and destination type of edge do not match");

  DFEdge *newDFEdge = DFEdge::Create(SrcDF, DestDF, EdgeType, SourcePosition,
                                     DestPosition, DestTy, isStreaming);

  HandleToDFEdgeMap[II] = newDFEdge;

  // Add Edge to the dataflow graph associated with the parent node
  N->addEdgeToDFGraph(newDFEdge);
}

void BuildDFG::handleBindInput(DFInternalNode *N, IntrinsicInst *II) {
  // The DFNode structures must be in the map before the edge is processed
  HandleToDFNode::iterator DFI = HandleToDFNodeMap.find(II->getOperand(0));
  assert(DFI != HandleToDFNodeMap.end());

  DFNode *SrcDF = N->getChildGraph()->getEntry();
  DFNode *DestDF = HandleToDFNodeMap[II->getOperand(0)];

  unsigned SourcePosition =
      cast<ConstantInt>(II->getOperand(1))->getZExtValue();
  unsigned DestPosition = cast<ConstantInt>(II->getOperand(2))->getZExtValue();

  bool isStreaming = !cast<ConstantInt>(II->getOperand(3))->isZero();

  // Get destination type
  FunctionType *FT = DestDF->getFuncPointer()->getFunctionType();
  assert((FT->getNumParams() > DestPosition) &&
         "Invalid argument number for destination dataflow node!");
  Type *DestTy = FT->getParamType(DestPosition);

  // Get source type
  FT = SrcDF->getFuncPointer()->getFunctionType();
  assert((FT->getNumParams() > SourcePosition) &&
         "Invalid argument number for parent dataflow node!");
  Type *SrcTy = FT->getParamType(SourcePosition);

  // check if the types are compatible
  assert(isTypeCongruent(SrcTy, DestTy) &&
         "Source and destination type of edge do not match");

  // Add Binding as an edge between Entry and child Node
  DFEdge *newDFEdge = DFEdge::Create(SrcDF, DestDF, false, SourcePosition,
                                     DestPosition, DestTy, isStreaming);

  HandleToDFEdgeMap[II] = newDFEdge;

  // Add Edge to the dataflow graph associated with the parent node
  N->addEdgeToDFGraph(newDFEdge);
}

void BuildDFG::handleBindOutput(DFInternalNode *N, IntrinsicInst *II) {
  // The DFNode structures must be in the map before the edge is processed
  HandleToDFNode::iterator DFI = HandleToDFNodeMap.find(II->getOperand(0));
  assert(DFI != HandleToDFNodeMap.end());

  DFNode *SrcDF = HandleToDFNodeMap[II->getOperand(0)];
  DFNode *DestDF = N->getChildGraph()->getExit();

  unsigned SourcePosition =
      cast<ConstantInt>(II->getOperand(1))->getZExtValue();
  unsigned DestPosition = cast<ConstantInt>(II->getOperand(2))->getZExtValue();

  bool isStreaming = !cast<ConstantInt>(II->getOperand(3))->isZero();

  // Get destination type
  StructType *OutTy = DestDF->getOutputType();
  assert((OutTy->getNumElements() > DestPosition) &&
         "Invalid argument number for destination parent dataflow node!");
  Type *DestTy = OutTy->getElementType(DestPosition);

  // Get source type
  OutTy = SrcDF->getOutputType();
  assert((OutTy->getNumElements() > SourcePosition) &&
         "Invalid argument number for source dataflow node!");
  Type *SrcTy = OutTy->getElementType(SourcePosition);

  // check if the types are compatible
  assert(isTypeCongruent(SrcTy, DestTy) &&
         "Source and destination type of edge do not match");

  // Add Binding as an edge between child and exit node
  DFEdge *newDFEdge = DFEdge::Create(SrcDF, DestDF, false, SourcePosition,
                                     DestPosition, DestTy, isStreaming);

  HandleToDFEdgeMap[II] = newDFEdge;

  // Add Edge to the dataflow graph associated with the parent node
  N->addEdgeToDFGraph(newDFEdge);
}

void BuildDFG::BuildGraph(DFInternalNode *N, Function *F) {
  DEBUG(errs() << "FUNCTION: " << F->getName() << "\n");
  // TODO: Place checks for valid hpvm functions. For example one of the
  // check can be that any function that contains hpvm dataflow graph
  // construction intrinsics should not have other llvm IR statements.

  // Iterate over all the instructions of a function and look for hpvm
  // intrinsics.
  for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
    Instruction *I = &*i; // Grab pointer to Instruction
    DEBUG(errs() << *I << "\n");
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(I)) {
      DEBUG(errs() << "IntrinsicID = " << II->getIntrinsicID() << ": "
                   << II->getCalledFunction()->getName() << "\n");
      switch (II->getIntrinsicID()) {
      case Intrinsic::hpvm_createNode:
      case Intrinsic::hpvm_createNode1D:
      case Intrinsic::hpvm_createNode2D:
      case Intrinsic::hpvm_createNode3D:
        handleCreateNode(N, II);
        break;
      case Intrinsic::hpvm_createEdge:
        handleCreateEdge(N, II);
        break;
      case Intrinsic::hpvm_bind_input:
        handleBindInput(N, II);
        break;
      case Intrinsic::hpvm_bind_output:
        handleBindOutput(N, II);
        break;

      // TODO: Reconsider launch within a dataflow graph (recursion?)
      case Intrinsic::hpvm_wait:
      case Intrinsic::hpvm_launch:
        DEBUG(errs()
              << "Error: Launch/wait intrinsic used within a dataflow graph\n\t"
              << *II << "\n");
        break;

      default:
        DEBUG(
            errs() << "Error: Invalid HPVM Intrinsic inside Internal node!\n\t"
                   << *II << "\n");
        break;
      }
      continue;
    }
    if (!isa<ReturnInst>(I) && !isa<CastInst>(I)) {
      DEBUG(errs() << "Non-intrinsic instruction: " << *I << "\n");
      llvm_unreachable("Found non-intrinsic instruction inside an internal "
                       "node. Only return instruction is allowed!");
    }
  }
}

char BuildDFG::ID = 0;
static RegisterPass<BuildDFG>
    X("buildDFG", "Hierarchical Dataflow Graph Builder Pass", false, false);

} // End of namespace builddfg
