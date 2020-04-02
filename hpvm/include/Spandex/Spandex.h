#ifndef __SPANDEX_H__
#define __SPANDEX_H__

#include <memory>
#include "llvm/Pass.h"

namespace spandex {

class Spandex : public llvm::ModulePass {
public:
  static char ID;

  Spandex();

  void getAnalysisUsage(AnalysisUsage &AU) const;

  virtual bool runOnModule(llvm::Module &M);

private:
  // https://cpppatterns.com/patterns/pimpl.html
  class impl;
  std::unique_ptr<impl> pImpl;
};
} // namespace spandex

#endif
