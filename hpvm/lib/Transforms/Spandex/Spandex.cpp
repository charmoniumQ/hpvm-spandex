/* Fast dry-run compile locally with

    clang-format -i lib/Transforms/Spandex/Spandex.cpp
    clang++ -std=c++11 lib/Transforms/Spandex/Spandex.cpp -Iinclude -Ibuild/include -Illvm_patches/include -Illvm/include

Anticipate errors related to intrinsics not found. Compile for real with

    ./scripts/hpvm_build.sh
*/

// https://llvm.org/docs/ProgrammersManual.html#fine-grained-debug-info-with-debug-type-and-the-debug-only-option
#include <numeric>
#define DEBUG_TYPE "Spandex"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "BuildDFG/BuildDFG.h"
#include "SupportHPVM/DFGraph.h"
#include "Spandex/Spandex.h"
#include "hpvm_util.hpp"
#include "spandex_util.hpp"

using namespace spandex;


class Spandex::impl {
public:
  Spandex &thisp;

  impl(Spandex &_thisp) : thisp{_thisp} {}

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<builddfg::BuildDFG>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<builddfg::BuildDFG>();
    AU.addPreserved<ScalarEvolutionWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
	AU.addRequired<AAResultsWrapperPass>();
  }

  bool runOnModule(Module &module) {
    LLVM_DEBUG(dbgs() << "vvvv Spandex vvvv\n");

    builddfg::BuildDFG &DFG = thisp.getAnalysis<builddfg::BuildDFG>();
    // DFG.getRoots() is not const unfortunately

    const std::vector<DFInternalNode *> roots = DFG.getRoots();

	bool active = false;
    if (!roots.empty()) {
	  active = true;
      for (const auto _root : roots) {
		  const auto& root = ptr2ref<DFInternalNode>(_root);
        Digraph<Port> dfg = get_dfg(root);
        DUMP_GRAPHVIZ_PORTS(dfg);

        Digraph<Port> leaf_dfg = get_leaf_dfg(dfg);
        DUMP_GRAPHVIZ_PORTS(leaf_dfg);

        Digraph<Ref<llvm::DFNode>> coarse_leaf_dfg = get_coarse_leaf_dfg(leaf_dfg);
        //DUMP_GRAPHVIZ(coarse_leaf_dfg);

        Digraph<Port> mem_comm_dfg = get_mem_comm_dfg(leaf_dfg, coarse_leaf_dfg);
        DUMP_GRAPHVIZ_PORT_PTRS(mem_comm_dfg);

		spandex_annotate(module, mem_comm_dfg);
      }
    }
	// No DFG. Nothing for us to do;
	LLVM_DEBUG(dbgs() << "^^^^ Spandex ^^^^\n");
	return active;
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
