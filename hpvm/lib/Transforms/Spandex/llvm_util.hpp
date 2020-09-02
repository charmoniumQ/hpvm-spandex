#pragma once
#include <utility>
#include "llvm/Support/Debug.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "enum.h"
#include "util.hpp"

llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const llvm::DFNode &N) {
  return stream << demangle(N.getFuncPointer()->getName().str())
                << (N.isEntryNode() ? ".entry" : N.isExitNode() ? ".exit" : "");
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              llvm::DFNode const *N) {
  return stream << *N;
}

template <typename First, typename Second>
llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const std::pair<First, Second> &pair) {
  return stream << pair.first << " : " << pair.second;
}

// template <typename Collection>
// llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
//                               const Collection &collection) {
//   stream << "[";
//   typename Collection::const_iterator it = collection.cbegin();
//   if (it != collection.cend()) {
//     stream << *it;
//     ++it;
//   }
//   while (it != collection.cend()) {
//     stream << ", " << *it;
//     ++it;
//   }
//   return stream << "]";
// }

BETTER_ENUM(AccessKind, unsigned char, access, store, load)

bool is_derived_from(const llvm::Value &Vsource, const llvm::Value &Vdest, bool verbose = false) {
	if (verbose) {
		LLVM_DEBUG(dbgs() << "is_derived_from: " << Vsource << " --?--> " << Vdest << "? ");
	}
  if (&Vsource == &Vdest) {
	  if (verbose) {
	  LLVM_DEBUG(dbgs() << "Yes.\n");
	  }
    return true;
  } else {
    const llvm::User *U;
    if ((U = dyn_cast<llvm::User>(&Vdest))) {
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
	if (verbose) {
		LLVM_DEBUG(dbgs() << "No.");
	}
    return false;
  }
}

bool is_access_to(const llvm::Instruction &I, const llvm::Value &Vptr, AccessKind AK, bool verbose = false) {
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

unsigned count_accesses(const llvm::BasicBlock &BB, const llvm::Value &Vptr, AccessKind AK,
                        llvm::ScalarEvolution &SE, const llvm::LoopInfo &LI, bool verbose = false) {
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

unsigned count_accesses(const llvm::Function &F, const llvm::Value &Vptr, AccessKind AK,
                        llvm::ScalarEvolution &SE, const llvm::LoopInfo &LI, bool verbose = false) {
  unsigned sum = 0;
  for (auto BB = F.begin(); BB != F.end(); ++BB) {
	  sum += count_accesses(*BB, Vptr, AK, SE, LI, verbose);
  }
  return sum;
}
