/* Fast dry-run compile locally with

    clang-format -i lib/Transforms/Spandex/Spandex.cpp
    clang++ lib/Transforms/Spandex/Spandex.cpp -Iinclude -Illvm_patches/include
-I/usr/include/llvm-c-9 -I/usr/include/llvm-9

Anticipate errors related to intrinsics not found. Compile for real with

    ./scripts/hpvm_build.sh
*/

#include <unordered_map>
#include <unordered_set>

// https://llvm.org/docs/ProgrammersManual.html#fine-grained-debug-info-with-debug-type-and-the-debug-only-option
#define DEBUG_TYPE "Spandex"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "BuildDFG/BuildDFG.h"
#include "SupportHPVM/DFGraph.h"
#include "Spandex/Spandex.h"
#include "type_util.hpp"
#include "graph_util.hpp"

using namespace llvm;
using namespace builddfg;
using namespace spandex;
using namespace std;

#define foreach(type, i, collection)                                           \
  for (type i = collection##begin(); i != collection##end(); ++i)

class get_edges_helper : public DFNodeVisitor {
private:
	std::unordered_map<Port, std::unordered_set<Port>> edges;

public:

  const DFNode *normalize(const DFNode *orig, bool src) {
    if (orig->getKind() == DFNode::InternalNode) {
      DFInternalNode *orig2 = const_cast<DFInternalNode *>(
          reinterpret_cast<const DFInternalNode *>(orig));
      DFGraph *g = orig2->getChildGraph();
      return src ? g->getExit() : g->getEntry();
    } else {
      return orig;
    }
  }

  const DFNode *normalize_dst(const DFNode *dst) {
    return isa<DFInternalNode>(dst)
               ? (reinterpret_cast<const DFInternalNode *>(dst))
                     ->getChildGraph()
                     ->getEntry()
               : dst;
  }

  virtual void visit2(const DFNode *N) {
    foreach (auto, edge, N->outdfedge_) {
      Port src{normalize((*edge)->getSourceDF(), true),
               (*edge)->getSourcePosition()};
      Port dst{normalize((*edge)->getDestDF(), false),
               (*edge)->getDestPosition()};
	  edges[src].insert(dst);
    }
  }

  virtual void visit(DFInternalNode *N) {
    visit2(reinterpret_cast<const DFNode *>(N));
    for (DFNode *child : *N->getChildGraph()) {
      child->applyDFNodeVisitor(*this);
    }
  }
  virtual void visit(DFLeafNode *N) {
    visit2(reinterpret_cast<const DFNode *>(N));
  }
	std::unordered_map<Port, std::unordered_set<Port>> get_edges() { return edges; }
};

std::unordered_map<Port, std::unordered_set<Port>> get_edges(const DFInternalNode *N) {
  get_edges_helper geh;
  // I know this visitor does not modify the graph a priori
  // but the graph visitor API (not owned by me) is marked as non-const
  const_cast<DFInternalNode *>(N)->applyDFNodeVisitor(geh);
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

    if (!roots.empty()) {
      for (const auto &root : roots) {
        auto edges = get_edges(root);

        LLVM_DEBUG(dbgs() << "edges:\n");
        LLVM_DEBUG(dump_graphviz(dbgs(), edges));

        // auto relaxed_edges =
        //     get_relaxed_edges<Port>(edges, [root](const Port &p) -> bool {
        //       return (p.N->getFuncPointer() == root->getFuncPointer()) ||
        //              p.N->getInstruction();
        //     });

        // LLVM_DEBUG(dbgs() << "relaxed edges:\n" << relaxed_edges << "\n");
      }
      return true;

    } else {
      // No DFG. Nothing for us to do;
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
