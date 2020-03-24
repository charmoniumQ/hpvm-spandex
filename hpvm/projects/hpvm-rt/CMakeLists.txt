add_definitions(-DNUM_CORES=8)

SET(CMAKE_C_COMPILER ${CMAKE_BINARY_DIR}/bin/clang)
SET(CMAKE_CXX_COMPILER ${CMAKE_BINARY_DIR}/bin/clang++)
SET(CMAKE_CXX_STANDARD 11)
# Defines ${OpenCL_INCLUDE_DIRS} and ${OpenCL_LIBRARY} if found
find_package(OpenCL REQUIRED)

add_llvm_library(hpvm-rt.ll hpvm-rt.cpp
  DEPENDS
  clang
  llvm-dis
)
target_compile_options(hpvm-rt.ll PUBLIC -flto)
target_include_directories(hpvm-rt.ll PRIVATE ${OpenCL_INCLUDE_DIRS})
link_directories(${OpenCL_LIBRARY})

add_custom_target(hpvm-rt.cpp.o ALL
  COMMAND ar -x ${CMAKE_BINARY_DIR}/lib/libhpvm-rt.ll.a
  COMMAND mv ${CMAKE_BINARY_DIR}/tools/hpvm/projects/hpvm-rt/hpvm-rt.cpp.o ${CMAKE_BINARY_DIR}/tools/hpvm/projects/hpvm-rt/hpvm-rt.bc
  COMMAND  ${CMAKE_BINARY_DIR}/bin/llvm-dis  ${CMAKE_BINARY_DIR}/tools/hpvm/projects/hpvm-rt/hpvm-rt.bc)

add_dependencies(hpvm-rt.cpp.o   hpvm-rt.ll)
