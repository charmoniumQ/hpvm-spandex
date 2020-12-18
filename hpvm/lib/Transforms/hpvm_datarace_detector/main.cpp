#include <iostream>
#include <sstream>
#include <fstream>
#define DEBUG_TYPE "alias_table"
#include "llvm/Support/Debug.h"
#include "llvm/Pass.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "BuildDFG/BuildDFG.h"
#include "util/llvm_util.hpp"
#include "util/hpvm_util.hpp"
#include "util/graph_util.hpp"

class hpvm_datarace_detector : public llvm::ModulePass {
public:
	static char ID;

	hpvm_datarace_detector() : ModulePass{ID} { }

	void getAnalysisUsage(llvm::AnalysisUsage &analysis_usage) const {
		analysis_usage.addRequired<builddfg::BuildDFG>();
		analysis_usage.addPreserved<builddfg::BuildDFG>();
		analysis_usage.addRequired<llvm::AAResultsWrapperPass>();
		analysis_usage.addPreserved<llvm::AAResultsWrapperPass>();
	}

	virtual bool runOnModule(llvm::Module &module) {
		builddfg::BuildDFG &DFG = getAnalysis<builddfg::BuildDFG>();
		// DFG.getRoots() is not const unfortunately

		const std::vector<DFInternalNode *> roots = DFG.getRoots();
		if (!roots.empty()) {
			for (const auto _root : roots) {
				const auto& root = ptr2ref<DFInternalNode>(_root);
				Digraph<Port> dfg = get_dfg(root);
				Digraph<Port> leaf_dfg = get_leaf_dfg(dfg);
				Digraph<Ref<llvm::DFNode>> coarse_leaf_dfg = get_coarse_leaf_dfg(leaf_dfg);
				Digraph<Port> mem_comm_dfg = get_mem_comm_dfg(leaf_dfg, coarse_leaf_dfg);
				std::unordered_map<Port, Port> arg_roots {get_roots_map<Port, Digraph<Port>>(mem_comm_dfg)};
				for (const auto& race : get_mayraces(coarse_leaf_dfg, mem_comm_dfg, arg_roots)) {
					std::cout << race.str();
				}
			}
		}
		return false;
	}
};

char hpvm_datarace_detector::ID = 0;

static llvm::RegisterPass<hpvm_datarace_detector> X{
    "hpvm_datarace_detector",
	"Detect dataraces in HPVM",
	false, /* Only looks at CFG */
    false, /* Analysis Pass */
};
