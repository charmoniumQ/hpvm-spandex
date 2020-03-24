#ifndef __DFGTREETRAVERSAL_H__
#define __DFGTREETRAVERSAL_H__

//=== DFGTreeTraversal.h - Header file for Tree Traversal of the HPVM DFG ====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/BuildDFG/BuildDFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace builddfg;

namespace dfg2llvm {

class DFGTreeTraversal : public DFNodeVisitor {

protected:
  // Member variables
  Module &M;
  BuildDFG &DFG;

  virtual void process(DFInternalNode *N) = 0;
  virtual void process(DFLeafNode *N) = 0;

  virtual ~DFGTreeTraversal() {}

public:
  // Constructor
  DFGTreeTraversal(Module &_M, BuildDFG &_DFG) : M(_M), DFG(_DFG) {}

  void visit(DFInternalNode *N) {
    // May visit a nodemore than once, there is no marking it as visited
    DEBUG(errs() << "Start: In Node (I) - " << N->getFuncPointer()->getName()
                 << "\n");

    // Follows a bottom-up approach.
    for (DFGraph::children_iterator i = N->getChildGraph()->begin(),
                                    e = N->getChildGraph()->end();
         i != e; ++i) {
      DFNode *child = *i;
      child->applyDFNodeVisitor(*this);
    }

    // Process this internal node now.
    process(N);
    DEBUG(errs() << "DONE: In Node (I) - " << N->getFuncPointer()->getName()
                 << "\n");
  }

  void visit(DFLeafNode *N) {
    DEBUG(errs() << "Start: In Node (L) - " << N->getFuncPointer()->getName()
                 << "\n");
    process(N);
    DEBUG(errs() << "DONE: In Node (L) - " << N->getFuncPointer()->getName()
                 << "\n");
  }
};

} // end namespace dfg2llvm

#endif
