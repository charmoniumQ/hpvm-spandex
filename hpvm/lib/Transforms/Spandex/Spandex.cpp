#include "Spandex/Spandex.h"
#include <memory>

using namespace llvm;

namespace spandex {

	class Spandex::impl {
	public:
		bool runOnModule(Module &M) {
			return false;
		}
	private:
	};

	bool Spandex::runOnModule(Module &M) {
		return pimpl->runOnModule(M);
	}

	Spandex::Spandex()
		: ModulePass{ID}
		, pimpl{std::make_unique<impl>()}
	{}
}
