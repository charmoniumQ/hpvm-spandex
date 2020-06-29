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
#include "enum.h"

using namespace spandex;

BETTER_ENUM(AccessKind, unsigned char, access, store, load)

bool is_derived_from(const Value &Vsource, const Value &Vdest, bool verbose = false) {
	if (verbose) {
		LLVM_DEBUG(dbgs() << "is_derived_from: " << Vsource << " --?--> " << Vdest << "? ");
	}
  if (&Vsource == &Vdest) {
	  if (verbose) {
	  LLVM_DEBUG(dbgs() << "Yes.\n");
	  }
    return true;
  } else {
    const User *U;
    if ((U = dyn_cast<User>(&Vdest))) {
      for (auto Vdest2 = U->value_op_begin(); Vdest2 != U->value_op_end();
           ++Vdest2) {
		  if (verbose) {
		  LLVM_DEBUG(dbgs() << "Through " << **Vdest2 << "?\n");
		  }
        if (is_derived_from(Vsource, **Vdest2)) {
          return true;
        }
		if (verbose) {
		LLVM_DEBUG(dbgs() << "is_derived_from: " << Vsource << " --?--> " << Vdest << "? ");
		}
      }
    }
		  LLVM_DEBUG(dbgs() << "No.");
    return false;
  }
}

bool is_access_to(const Instruction &I, const Value &Vptr, AccessKind AK, bool verbose = false) {
  const LoadInst *LI = nullptr;
  const StoreInst *SI = nullptr;
  if (false) {
  } else if ((AK == (+AccessKind::access) || AK == (+AccessKind::load )) &&
             (LI = dyn_cast<LoadInst>(&I))) {
    const Value &V = *LI->getPointerOperand();
    if (is_derived_from(Vptr, V)) {
      return true;
    }
  } else if ((AK == (+AccessKind::access) || AK == (+AccessKind::store)) &&
             (SI = dyn_cast<StoreInst>(&I))) {
    const Value &V = *SI->getPointerOperand();
    if (is_derived_from(Vptr, V)) {
      return true;
    }
  }
  return false;
}

unsigned count_accesses(const BasicBlock &BB, const Value &Vptr, AccessKind AK,
                        ScalarEvolution &SE, const LoopInfo &LI, bool verbose = false) {
  unsigned sum = 0;
  for (auto I = BB.begin(); I != BB.end(); ++I) {
	unsigned accesses = 0;
    if (is_access_to(*I, Vptr, AK, verbose)) {
	  accesses = 1;
      const Loop *loop = LI.getLoopFor(&BB);
      while (loop != nullptr) {
        accesses *= SE.getSmallConstantTripCount(loop);
        loop = loop->getParentLoop();
      }
	  if (verbose) {
		  LLVM_DEBUG(dbgs() << "count_accesses: " << accesses << " x " << *I << "\n");
	  }
    }
    sum += accesses;
  }
  return sum;
}

unsigned count_accesses(const Function &F, const Value &Vptr, AccessKind AK,
                        ScalarEvolution &SE, const LoopInfo &LI, bool verbose = false) {
  unsigned sum = 0;
  for (auto BB = F.begin(); BB != F.end(); ++BB) {
	  sum += count_accesses(*BB, Vptr, AK, SE, LI, verbose);
  }
  return sum;
}

BETTER_ENUM(SpandexRequestType, char, N, Odata, O, S, V, Vo, WT, WTo, WTfwd)

void assign_request_types(const Function &F, const Argument& Arg, float requests_fraction, bool consumer, hpvm::Target target, bool owner_pred) {
	typedef SpandexRequestType Req;
	for (auto BB = F.begin(); BB != F.end(); ++BB) {
		bool has_load = false;
		for (auto I = BB->begin(); I != BB->end(); ++I) {
			Req SRT = Req::N;
			if (is_access_to(*I, Arg, AccessKind::load)) {
				has_load = true;
				if (consumer) {
					SRT = Req::Odata;
				} else /* producer */ {
					if (requests_fraction > 0.5) {
						SRT = Req::Odata;
					} else if (target == hpvm::CPU_TARGET) {
						SRT = Req::S;
					} else if (target == hpvm::GPU_TARGET) {
						if (owner_pred) {
							SRT = Req::Vo;
						} else {
							SRT = Req::V;
						}
					}
				}
			}
			if (SRT != (+Req::N)) {
				LLVM_DEBUG(dbgs() << "assign_request_types: " << *I << " Req" << SRT._to_string() << "\n");
			}
		}
		for (auto I = BB->begin(); I != BB->end(); ++I) {
			Req SRT = Req::N;
			if (is_access_to(*I, Arg, AccessKind::store)) {
				if (has_load) {
					SRT = Req::Odata;
				} else {
					SRT = Req::O;
				}
			}
			if (SRT != (+Req::N)) {
				LLVM_DEBUG(dbgs() << "assign_request_types: " << *I << " Req" << SRT._to_string() << "\n");
			}
		}
	}
}

void assign_request_types2(const Function &F, const Argument& Arg, float requests_fraction, bool consumer, hpvm::Target target, bool owner_pred) {
	typedef SpandexRequestType Req;
	for (auto BB = F.begin(); BB != F.end(); ++BB) {
		for (auto I = BB->begin(); I != BB->end(); ++I) {
			Req SRT = Req::N;
			if (is_access_to(*I, Arg, AccessKind::load)) {
				if (consumer) {
					SRT = Req::Odata;
				} else /* producer */{
					if (requests_fraction > 0.5) {
						SRT = Req::Odata;
					} else if (target == hpvm::CPU_TARGET) {
						SRT = Req::S;
					} else if (target == hpvm::GPU_TARGET) {
						if (owner_pred) {
							SRT = Req::Vo;
						} else {
							SRT = Req::V;
						}
					}
				}
			} else if (is_access_to(*I, Arg, AccessKind::store)) {
				if (consumer) {
					SRT = Req::Odata;
				} else /* producer */ {
					if (owner_pred) {
						SRT = Req::WTo;
					} else {
						SRT = Req::WTfwd;
					}
				}
			}
			if (SRT != (+Req::N)) {
				LLVM_DEBUG(dbgs() << "assign_request_types: " << *I << " Req" << SRT._to_string() << "\n");
			}
		}
	}
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
    LLVM_DEBUG(dbgs() << "vvvv Spandex vvvv\n");

    builddfg::BuildDFG &DFG = thisp.getAnalysis<builddfg::BuildDFG>();
    // DFG.getRoots() is not const

    const std::vector<DFInternalNode *> roots = DFG.getRoots();

	bool active = false;
    if (!roots.empty()) {
	  active = true;
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
              assert(consumers.size() <= 1);
              if (consumers.size() == 1) {
                const Port &consumer = *consumers.cbegin();
                assert(producer.N->getFuncPointer());
                assert(consumer.N->getFuncPointer());

				ScalarEvolution &consumerSE =
					thisp.getAnalysis<ScalarEvolutionWrapperPass>(*consumer.N->getFuncPointer()).getSE();
				// SE.getSmallConstantTripCount(const Loop*) is not const

				const LoopInfo &consumerLI = thisp.getAnalysis<LoopInfoWrapperPass>(*consumer.N->getFuncPointer()).getLoopInfo();

				ScalarEvolution &producerSE =
					thisp.getAnalysis<ScalarEvolutionWrapperPass>(*producer.N->getFuncPointer()).getSE();
				// SE.getSmallConstantTripCount(const Loop*) is not const

				const LoopInfo &producerLI = thisp.getAnalysis<LoopInfoWrapperPass>(*producer.N->getFuncPointer()).getLoopInfo();

				const Argument& producer_arg = *(producer.N->getFuncPointer()->arg_begin() + producer.pos);
                unsigned producer_accesses = count_accesses(
														*producer.N->getFuncPointer(), producer_arg,
                    AccessKind::access, producerSE, producerLI);
				const Argument& consumer_arg = *(consumer.N->getFuncPointer()->arg_begin() + consumer.pos);
                unsigned consumer_accesses = count_accesses(
                    *consumer.N->getFuncPointer(),
                    consumer_arg,
                    AccessKind::access, consumerSE, consumerLI);
				unsigned total_accesses = producer_accesses + consumer_accesses;

				assign_request_types2(*producer.N->getFuncPointer(), producer_arg, float(producer_accesses) / float(total_accesses), false, hpvm::CPU_TARGET, true);
				assign_request_types2(*consumer.N->getFuncPointer(), consumer_arg, float(consumer_accesses) / float(total_accesses), true, hpvm::CPU_TARGET, true);
              }
            });
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
