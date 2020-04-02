/* Fast dry-run compile locally with

    clang-format -i lib/Transforms/Spandex/Spandex.cpp && clang++
lib/Transforms/Spandex/Spandex.cpp -Iinclude -Illvm_patches/include
-I/usr/include/llvm-c-9 -I/usr/include/llvm-9

Anticipate errors related to intrinsics not found. Compile for real with

    ./scripts/hpvm_build.sh

*/

#include <memory>
#include <unordered_set>

// https://llvm.org/docs/ProgrammersManual.html#fine-grained-debug-info-with-debug-type-and-the-debug-only-option
#define DEBUG_TYPE "Spandex"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "BuildDFG/BuildDFG.h"
#include "SupportHPVM/DFGraph.h"
#include "Spandex/Spandex.h"

using namespace std;
using namespace llvm;
using namespace builddfg;
using namespace spandex;

#define foreach(type, i, collection)                                           \
  for (type i = collection##begin(); i != collection##end(); ++i)

string to_string(const DFNode *N) { return N->getFuncPointer()->getName(); }

string to_string(const DFEdge *E) {
  return to_string(E->getSourceDF()) + "["s +
         to_string(E->getSourcePosition()) + "] -> "s +
         to_string(E->getDestDF()) + "["s + to_string(E->getDestPosition()) +
         "]"s;
}

class get_edges_helper : public DFNodeVisitor {
private:
  std::unordered_set<const DFEdge *> edges;

public:
  virtual void visit(DFInternalNode *N) {
    for (DFNode *child : *N->getChildGraph()) {
      child->applyDFNodeVisitor(*this);
    }
    foreach (auto, edge, N->indfedge_) { edges.insert(*edge); }
    foreach (auto, edge, N->outdfedge_) { edges.insert(*edge); }
  }
  virtual void visit(DFLeafNode *N) {
    foreach (auto, edge, N->indfedge_) { edges.insert(*edge); }
    foreach (auto, edge, N->outdfedge_) { edges.insert(*edge); }
  }
  std::unordered_set<const DFEdge *> get_edges() { return edges; }
};

std::unordered_set<const DFEdge *> get_edges(DFInternalNode *N) {
  get_edges_helper geh;
  N->applyDFNodeVisitor(geh);
  return geh.get_edges();
}

class Spandex::impl {
public:
  Spandex &thisp;

  impl(Spandex &_thisp) : thisp{_thisp} {}

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<BuildDFG>();
    AU.addPreserved<BuildDFG>();
  }

  bool runOnModule(Module &M) {
    LLVM_DEBUG(dbgs() << "\nSpandex pass\n");

    BuildDFG &DFG = thisp.getAnalysis<BuildDFG>();

    std::vector<DFInternalNode *> roots = DFG.getRoots();

    assert(roots.size() <= 1 &&
           "This pass doesn't work for more than one HPVM DFG");

    if (roots.size() == 1) {
      auto edges = get_edges(roots[0]);
      LLVM_DEBUG(dbgs() << "edges:\n");
      for (auto edge : edges) {
        LLVM_DEBUG(dbgs() << to_string(edge) << "\n");
      }
      return true;
    } else {
      return false;
    }
  }
};

bool Spandex::runOnModule(Module &M) { return pImpl->runOnModule(M); }

void Spandex::getAnalysisUsage(AnalysisUsage &AU) const {
  pImpl->getAnalysisUsage(AU);
}

Spandex::Spandex() : ModulePass{ID}, pImpl{std::make_unique<impl>(*this)} {}

char Spandex::ID = 0;

static RegisterPass<Spandex> X{
    "spandex", "Generate Spandex Hints Pass", false /* Only looks at CFG */,
    false /* Analysis Pass */
};
