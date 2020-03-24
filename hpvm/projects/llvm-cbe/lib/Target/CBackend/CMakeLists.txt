#add_llvm_target(CBackendCodeGen
#  CBackend.cpp
#  )
#
#add_subdirectory(TargetInfo)
add_subdirectory(TargetInfo)

set(LLVM_LINK_COMPONENTS
  Analysis
  CBackendInfo
  CodeGen
  Core
  MC
  ScalarOpts
  Support
  Target
  SelectionDAG
  TransformUtils
  )

add_llvm_library(LLVMCBackendCodeGen
  CBackend.cpp

  DEPENDS
  intrinsics_gen
  )

#add_dependencies(CBackendCodeGen intrinsics_gen)
