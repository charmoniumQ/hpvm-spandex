//===------------ HPVMTimer.h - Header file for "HPVM Timer API" ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef HPVM_HINT_HEADER
#define HPVM_HINT_HEADER

/************************** Hint Routines ***************************/
#ifdef __cplusplus
namespace hpvm {
#endif

enum Target {
  None,
  CPU_TARGET,
  GPU_TARGET,
  SPIR_TARGET,
  CUDNN_TARGET,
  PROMISE_TARGET,
  CPU_OR_GPU_TARGET,
  CPU_OR_SPIR_TARGET,
  //    ALL_TARGETS,
  NUM_TARGETS
};

#ifdef __cplusplus
}
#endif

#endif // HPVM_HINT_HEADER
