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

if [ ! -f "${BUILD_DIR}/Makefile" ]; then
	# For building in Nix on dholak:
	# OpenCL_DIR="$(find /nix/store -maxdepth 1 -name '*-opencl-headers-22-2017-07-18')"
	# ocl_icd="$(find /nix/store -maxdepth 1 -name '*-ocl-icd-2.2.10')"
	# cmake_args=-DOpenCL_INCLUDE_DIR="${OpenCL_DIR}/include" -DOpenCL_LIBRARY="${ocl_icd}/lib"

	# For building natively on dholak:
	# cmake_args=-DPYTHON_EXECUTABLE:FILEPATH=$(which python3)

	# cmake_args="${cmake_args} -B${BUILD_DIR}"

	cd build
	cmake "../${LLVM_SRC}" -DLLVM_TARGETS_TO_BUILD="X86" -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" ${cmake_args}
	cd ..
fi

make -j${NUM_THREADS} -C "${BUILD_DIR}"

build/bin/llvm-lit -v test/regressionTests
