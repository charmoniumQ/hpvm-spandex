/* Fast dry-run compile locally with

    clang++ lib/Transforms/Spandex/Spandex.cpp -Iinclude -Illvm_patches/include -I/usr/include/llvm-9 -I/usr/include/llvm-c-9

Anticipate errors related to intrinsics not found. Compile for real with 

    ./scripts/hpvm_build.sh

*/

#include <memory>

// https://llvm.org/docs/ProgrammersManual.html#fine-grained-debug-info-with-debug-type-and-the-debug-only-option
#define DEBUG_TYPE "Spandex"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "BuildDFG/BuildDFG.h"
#include "SupportHPVM/DFGraph.h"
#include "Spandex/Spandex.h"

using namespace llvm;
using namespace builddfg;
using namespace spandex;

class Spandex::impl {
public:
	Spandex& thisp;

	impl(Spandex& _thisp)
		: thisp{_thisp}
	{}

	void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.addRequired<BuildDFG>();
		AU.addPreserved<BuildDFG>();
	}

	bool runOnModule(Module& M) {
		LLVM_DEBUG(dbgs() << "Spandex pass\n ");

		BuildDFG &DFG = thisp.getAnalysis<BuildDFG>();

		std::vector<DFInternalNode*> Roots = DFG.getRoots();

		// CGT_CPU *CGTVisitor = new CGT_CPU{M, DFG};
		// class CGT_CPU : public CodeGenTraversal

		// for (auto &rootNode : Roots) {
		// 	// Initiate code generation for root DFNode
		// 	CGTVisitor->visit(rootNode);
		// 	if (rootNode->isChildGraphStreaming()) {
		// 		CGTVisitor->codeGenLaunchStreaming(rootNode);
		// 	} else {
		// 		CGTVisitor->codeGenLaunch(rootNode);
		// 	}
		// }

		return false;
	}
};

bool Spandex::runOnModule(Module& M) {
	return pImpl->runOnModule(M);
}

void Spandex::getAnalysisUsage(AnalysisUsage &AU) const {
	pImpl->getAnalysisUsage(AU);
}

Spandex::Spandex()
	: ModulePass{ID}
	, pImpl{std::make_unique<impl>(*this)}
{}

char Spandex::ID = 0;

static RegisterPass<Spandex> X {
	"spandex",
	"Generate Spandex Hints Pass",
	false /* Only looks at CFG */,
	false /* Analysis Pass */
};
