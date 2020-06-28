/* Fast dry-run compile locally with

    clang-format -i lib/Transforms/Spandex/Spandex.cpp
    clang++ -std=c++11 lib/Transforms/Spandex/Spandex.cpp -Iinclude
-Ibuild/include -Illvm_patches/include -Illvm/include

Anticipate errors related to intrinsics not found. Compile for real with

    ./scripts/hpvm_build.sh
*/

#include <numeric>

// https://llvm.org/docs/ProgrammersManual.html#fine-grained-debug-info-with-debug-type-and-the-debug-only-option
#define DEBUG_TYPE "Spandex"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "BuildDFG/BuildDFG.h"
#include "SupportHPVM/DFGraph.h"
#include "Spandex/Spandex.h"
#include "type_util.hpp"
#include "graph_util.hpp"
#include "dfg_util.hpp"

using namespace spandex;

enum class access_kind {
  access,
  load,
  store,
};

bool is_derived_from(const Value &Vsource, const Value &Vdest) {
  if (&Vsource == &Vdest) {
    return true;
  } else {
    const User *U;
    if ((U = dyn_cast<User>(&Vdest))) {
      for (auto Vdest2 = U->value_op_begin(); Vdest2 != U->value_op_end();
           ++Vdest2) {
        if (is_derived_from(Vsource, **Vdest2)) {
          return true;
        }
      }
    }
    return false;
  }
}

unsigned count_accesses(const Instruction &I, const Value &Vptr, access_kind AK,
                        ScalarEvolution &SE, const LoopInfo &) {
  const LoadInst *LI = nullptr;
  const StoreInst *SI = nullptr;
  if (false) {
  } else if ((AK == access_kind::access || AK == access_kind::store) &&
             (LI = dyn_cast<LoadInst>(&I))) {
    const Value &V = *LI->getPointerOperand();
    if (is_derived_from(Vptr, V)) {
      return 1;
    }
  } else if ((AK == access_kind::access || AK == access_kind::store) &&
             (SI = dyn_cast<StoreInst>(&I))) {
    const Value &V = *SI->getPointerOperand();
    if (is_derived_from(Vptr, V)) {
      return 1;
    }
  }
  return 0;
}

unsigned count_accesses(const BasicBlock &BB, const Value &Vptr, access_kind AK,
                        ScalarEvolution &SE, const LoopInfo &LI) {
  unsigned sum = 0;
  for (auto I = BB.begin(); I != BB.end(); ++I) {
    unsigned accesses = count_accesses(*I, Vptr, AK, SE, LI);
    const Loop *loop = LI.getLoopFor(&BB);
    if (accesses != 0) {
      while (loop != nullptr) {
        accesses *= SE.getSmallConstantTripCount(loop);
        loop->getParentLoop();
      }
    }
    sum += accesses;
  }
  return sum;
}

unsigned count_accesses(const Function &F, const Value &Vptr, access_kind AK,
                        ScalarEvolution &SE, const LoopInfo &LI) {
  unsigned sum = 0;
  for (auto BB = F.begin(); BB != F.end(); ++BB) {
    sum += count_accesses(*BB, Vptr, AK, SE, LI);
  }
  return sum;
}

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
  }

  bool runOnModule(Module &M) {
    LLVM_DEBUG(dbgs() << "Spandex pass start\n");

    ScalarEvolution &SE =
        thisp.getAnalysis<ScalarEvolutionWrapperPass>().getSE();
    // SE.getSmallConstantTripCount(const Loop*) is not const

    const LoopInfo &LI = thisp.getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

    builddfg::BuildDFG &DFG = thisp.getAnalysis<builddfg::BuildDFG>();
    // DFG.getRoots() is not const

    const std::vector<DFInternalNode *> roots = DFG.getRoots();

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
            map_graph<Port, DFNode const *>(
                leaf_dfg, [](const Port &port) { return port.N; });
        DUMP_GRAPHVIZ(coarse_leaf_dfg);

        digraph<Port> mem_comm_dfg =
            get_mem_comm_dfg(leaf_dfg, coarse_leaf_dfg);
        DUMP_GRAPHVIZ_PORT_PTRS(mem_comm_dfg);

        for_each_adj_list<Port>(
            mem_comm_dfg,
            [&](const Port &producer, const adj_list<Port> &consumers) {
              if (consumers.size() > 1) {
                errs() << "Spandex pass only works when there is not more than "
                          "one consumer.\n";
              }
              assert(consumers.size() < 1);
              if (consumers.size() == 1) {
                const Port &consumer = *consumers.cbegin();
                assert(producer.N->getFuncPointer());
                assert(consumer.N->getFuncPointer());

                count_accesses(
                    *producer.N->getFuncPointer(),
                    *(producer.N->getFuncPointer()->arg_begin() + producer.pos),
                    access_kind::access, SE, LI);
                count_accesses(
                    *consumer.N->getFuncPointer(),
                    *(consumer.N->getFuncPointer()->arg_begin() + consumer.pos),
                    access_kind::access, SE, LI);
              }
            });
      }
      LLVM_DEBUG(dbgs() << "Spandex pass done\n");
      return true;

    } else {
      // No DFG. Nothing for us to do;
      LLVM_DEBUG(dbgs() << "Spandex pass done\n");
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
