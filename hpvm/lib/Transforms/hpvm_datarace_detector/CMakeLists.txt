add_llvm_library( hpvm_datarace_detector
  MODULE
  main.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )
