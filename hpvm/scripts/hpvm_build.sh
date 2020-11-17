#!/bin/bash

set -e -x
cd "$(dirname "${0}")/.."

VERSION="9.0.0"
BUILD_DIR="$(pwd)/build"
INSTALL_DIR="$(pwd)/install"
NUM_THREADS=$(python -c "import multiprocessing as mp; print(mp.cpu_count()//2)")

LLVM_SRC=llvm
CLANG_SRC="${LLVM_SRC}/tools/clang"
HPVM_SRC="${LLVM_SRC}/tools/hpvm"

if [ ! -d "${LLVM_SRC}" ]; then
	src_dir="llvm-${VERSION}.src"
	src_archive="${src_dir}.tar.xz"

	wget "http://releases.llvm.org/${VERSION}/${src_archive}"
    tar xf "${src_archive}"
    rm "${src_archive}"
	mv "${src_dir}" "${LLVM_SRC}"
fi

if [ ! -d "${CLANG_SRC}" ]; then
	src_dir="cfe-${VERSION}.src"
	src_archive="${src_dir}.tar.xz"

	oldwd="${PWD}"
	cd "$(dirname "${CLANG_SRC}")"
    wget "http://releases.llvm.org/${VERSION}/${src_archive}"
    tar xf "${src_archive}"
    rm "${src_archive}"
	mv "${src_dir}" "$(basename "${CLANG_SRC}")"
	cd "${oldwd}"
fi

if [ ! -d $HPVM_SRC ]; then
	mkdir -p $HPVM_SRC

	files=(CMakeLists.txt include lib projects test tools)
	for file in ${files[*]}; do
		ln -s "${PWD}/${file}" "${HPVM_SRC}/${file}"
	done

	find llvm_patches/ -mindepth 2 -type f -print0 | 
		while IFS= read -r -d '' file; do
			realfile="$(echo ${file} | cut -d'/' -f2-)"
			#rm -f "${LLVM_SRC}/${realfile}"
			cp "llvm_patches/${realfile}" "${LLVM_SRC}/${realfile}"
	done

fi

mkdir -p "${BUILD_DIR}"

if [ ! -f "${BUILD_DIR}/build.ninja" ]; then
	# For building in Nix on dholak:
	# OpenCL_DIR="$(find /nix/store -maxdepth 1 -name '*-opencl-headers-22-2017-07-18')"
	# ocl_icd="$(find /nix/store -maxdepth 1 -name '*-ocl-icd-2.2.10')"
	# cmake_args="${cmake_args} -D OpenCL_INCLUDE_DIR=${OpenCL_DIR}/include -D OpenCL_LIBRARY=${ocl_icd}/lib"

	# For building natively on dholak:
	# cmake_args="${cmake_args} -D PYTHON_EXECUTABLE:FILEPATH=$(which python3)"

	cmake \
		-S "${LLVM_SRC}" \
		-B "${BUILD_DIR}" \
		-G Ninja \
		-D LLVM_TARGETS_TO_BUILD="X86" \
		-D CMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
		-D CMAKE_EXE_LINKER_FLAGS="-fuse-ld=gold" \
		-D CMAKE_SHARED_LINKER_FLAGS="-fuse-ld=gold" \
		${cmake_args}
fi

#make -j "${NUM_THREADS}" -C "${BUILD_DIR}"
ninja -C "${BUILD_DIR}"

#build/bin/llvm-lit -v test/regressionTests

make -C test/benchmarks/simpler
