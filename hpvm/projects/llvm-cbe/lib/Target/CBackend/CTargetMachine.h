//===-- CTargetMachine.h - TargetMachine for the C backend ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the TargetMachine that is used by the C backend.
//
//===----------------------------------------------------------------------===//

#ifndef CTARGETMACHINE_H
#define CTARGETMACHINE_H

#include "llvm/ADT/Optional.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

struct CTargetMachine : public TargetMachine {

  // NOTE: Interface change
  CTargetMachine(const Target &T, const Triple &TargetTriple, StringRef CPU,
                 StringRef FS, const TargetOptions &Options,
                 Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                 CodeGenOpt::Level OL, bool JIT)

      : TargetMachine(T, "", TargetTriple, CPU, FS, Options) {}

  /// Add passes to the specified pass manager to get the specified file
  /// emitted.  Typically this will involve several steps of code generation.

  /*bool addPassesToEmitFile(
    PassManagerBase &PM, raw_pwrite_stream &Out, CodeGenFileType FileType,
    bool DisableVerify = true, AnalysisID StartBefore = nullptr,
    AnalysisID StartAfter = nullptr, AnalysisID StopBefore = nullptr,
    AnalysisID StopAfter = nullptr) override;
    //MachineFunctionInitializer *MFInitializer = nullptr) override;
  */

  virtual bool addPassesToEmitFile(PassManagerBase &PM, raw_pwrite_stream &Out,
                                   raw_pwrite_stream *Out2,
                                   CodeGenFileType FileType,
                                   bool DisableVerify = true,
                                   MachineModuleInfo *MMI = nullptr) override;
};

extern Target TheCBackendTarget;

} // namespace llvm

#endif
