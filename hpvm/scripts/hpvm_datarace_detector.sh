#!/bin/bash

set -e -x

source_cpp="${1}"
source="${source_cpp/%.cpp}"
HPVM_ROOT="$(realpath "$(dirname "$(realpath "${0}")")/..")"
LLVM_BUILD_DIR="${HPVM_ROOT}/build"
HPVM_RT_BC="${LLVM_BUILD_DIR}/tools/hpvm/projects/hpvm-rt/hpvm-rt.bc"
IFLAGS="-I${HPVM_ROOT}/test/benchmarks/include -I${HPVM_ROOT}/include"
CXXFLAGS="-ffast-math -O3 -fno-lax-vector-conversions -fno-vectorize -fno-slp-vectorize"
LDFLAGS="-L/usr/local/lib -lOpenCL -lpthread"

if [ ! -f "${source_cpp}" ]; then
	echo "${source_cpp} does not exist"
	exit 1
fi

if [ "${source_cpp:(-4)}" != ".cpp" ]; then
	echo "Source must be a .cpp file"
	exit 1
fi

"${HPVM_ROOT}/scripts/hpvm_build.sh"

"${LLVM_BUILD_DIR}/bin/clang++" \
    "${source}.cpp" \
	${CXXFLAGS} \
	${IFLAGS} \
    -emit-llvm -S -o "${source}.ll"

"${LLVM_BUILD_DIR}/bin/opt" \
    "${source}.ll" \
	-load LLVMGenHPVM.so \
	-genhpvm \
    -globaldce \
	-load LLVMBuildDFG.so -buildDFG \
    -debug-only=hpvm_datarace_detector -load hpvm_datarace_detector.so -hpvm_datarace_detector \
    -S -o "${source}.hpvm.ll"

"${LLVM_BUILD_DIR}/bin/opt" \
    "${source}.hpvm.ll" \
    -verify \
    -load LLVMBuildDFG.so -load LLVMDFG2LLVM_CPU.so -dfg2llvm-cpu \
    -load LLVMClearDFG.so -clearDFG \
    -S -o "${source}.host.ll"

    # -debug-only=copy_dfg -load copy_dfg.so -copy_dfg \
    # -debug-only=alias_table -load alias_table.so -alias_table \

# skip generating ${source}.linked.ll
"${LLVM_BUILD_DIR}/bin/llvm-link" \
    "${source}.host.ll" \
     "${HPVM_RT_BC}" \
    -S -o "${source}.linked.ll"
"${LLVM_BUILD_DIR}/bin/clang++" \
	"${source}.linked.ll" \
	${LDFLAGS} \
	-O3 \
	-o "${source}.exe"

# "${LLVM_BUILD_DIR}/bin/clang++" \
# 	"${source}.host.ll" \
# 	"${HPVM_RT_BC}" \
# 	${LDFLAGS} \
# 	-O3 \
# 	-o "${source}.exe"
