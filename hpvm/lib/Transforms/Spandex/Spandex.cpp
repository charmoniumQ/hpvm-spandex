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
#include "dfg_util.hpp"

using namespace spandex;

class Spandex::impl {
public:
  Spandex &thisp;

  impl(Spandex &_thisp) : thisp{_thisp} {}

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<builddfg::BuildDFG>();
    AU.addPreserved<builddfg::BuildDFG>();
  }

  bool runOnModule(Module &M) {
    LLVM_DEBUG(dbgs() << "\nSpandex pass\n");

    builddfg::BuildDFG &DFG = thisp.getAnalysis<builddfg::BuildDFG>();

    std::vector<DFInternalNode *> roots = DFG.getRoots();

    if (!roots.empty()) {
      for (const auto &root : roots) {
        digraph<Port> dfg = get_dfg(root);
        DUMP_GRAPHVIZ_PORTS(dfg);

        digraph<Port> leaf_dfg{dfg}; // copy
        delete_nodes<Port>(leaf_dfg, [](const Port &port) {
          return port.N->isDummyNode() && port.N->getLevel() != 1;
        });
        DUMP_GRAPHVIZ_PORTS(leaf_dfg);

        digraph<DFNode const *> coarse_leaf_dfg =
            map_graph<Port, DFNode const *>(leaf_dfg,
                                            [](auto port) { return port.N; });
        DUMP_GRAPHVIZ(coarse_leaf_dfg);

        digraph<Port> mem_comm_dfg =
            get_mem_comm_dfg(leaf_dfg, coarse_leaf_dfg);
        DUMP_GRAPHVIZ_PORTS(mem_comm_dfg);

        // for (const auto& producer_consumer : ptr_aware_leaf_dfg) {
        // 	auto producer = producer_consumer.first;
        // 	auto consumer = producer_consumer.second;
        // }
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
