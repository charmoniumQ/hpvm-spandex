if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLLVM_BUILD_DIR=${PROJECT_BINARY_DIR}")

add_llvm_library( LLVMGenHPVM
  MODULE
  GenHPVM.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )
