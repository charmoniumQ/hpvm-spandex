#ifndef __SPANDEX_H__
#define __SPANDEX_H__

#include <memory>
#include "llvm/Pass.h"

namespace spandex {

	class Spandex : public llvm::ModulePass {
	public:

		static char ID;

		Spandex();

		virtual bool runOnModule(llvm::Module &M);

	private:
		class impl;
		std::unique_ptr<impl> pimpl;
	};
}

#endif
