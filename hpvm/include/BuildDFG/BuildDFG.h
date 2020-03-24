#ifndef __BUILD_DFG_H__
#define __BUILD_DFG_H__

//== BuildDFG.h - Header file for "Hierarchical Dataflow Graph Builder Pass" =//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass defines the BuildDFG pass which uses LLVM IR with HPVM intrinsics
// to infer information about dataflow graph hierarchy and structure to
// construct HPVM IR.
//
//===----------------------------------------------------------------------===//

#include "SupportHPVM/DFGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace builddfg {
// BuildDFG - The first implementation.
struct BuildDFG : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  BuildDFG() : ModulePass(ID) {}

  typedef ValueMap<Value *, DFNode *> HandleToDFNode;
  typedef ValueMap<Value *, DFEdge *> HandleToDFEdge;

private:
  // Member variables
  DFInternalNode *Root;
  std::vector<DFInternalNode *> Roots;

  HandleToDFNode HandleToDFNodeMap; // This map associates the i8* pointer
  // with the DFNode structure that it
  // represents
  HandleToDFEdge HandleToDFEdgeMap; // This map associates the i8* pointer
  // with the DFEdge structure that it
  // represents

  // Functions
public:
  void handleCreateNode(DFInternalNode *N, IntrinsicInst *II);

private:
  void handleCreateEdge(DFInternalNode *N, IntrinsicInst *II);
  void handleGetParentNode(DFInternalNode *N, IntrinsicInst *II);
  void handleBindInput(DFInternalNode *N, IntrinsicInst *II);
  void handleBindOutput(DFInternalNode *N, IntrinsicInst *II);

  void BuildGraph(DFInternalNode *N, Function *F);

public:
  // Functions
  virtual bool runOnModule(Module &M);

  static bool isHPVMLaunchIntrinsic(Instruction *I);
  static bool isHPVMGraphIntrinsic(Instruction *I);
  static bool isHPVMQueryIntrinsic(Instruction *I);
  static bool isHPVMIntrinsic(Instruction *I);
  static bool isTypeCongruent(Type *L, Type *R);

  // TODO: Maybe make these fields const
  DFInternalNode *getRoot() const;
  std::vector<DFInternalNode *> &getRoots();
  HandleToDFNode &getHandleToDFNodeMap();
  HandleToDFEdge &getHandleToDFEdgeMap();
  void addElementToHandleToDFNodeMap(Value *V, DFNode *N);
  void removeElementFromHandleToDFNodeMap(Value *V);
  void addElementToHandleToDFEdgeMap(Value *V, DFEdge *E);
  void removeElementFromHandleToDFEdgeMap(Value *V);
};

} // namespace builddfg

#endif
