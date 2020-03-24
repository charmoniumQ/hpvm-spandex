# The HPVM Compiler Infrastructure

This repository contains the source code and documentation for the HPVM Compiler Infrastructure.

The README briefly describes how to get started with building and installing HPVM. It also provides a
benchmark suite to test the compiler infrastructure.

HPVM is currently at version 0.5. For more about what HPVM is, see [our website](https://publish.illinois.edu/hpvm-project/).

## Paper

[PPoPP'18 paper](https://dl.acm.org/doi/pdf/10.1145/3200691.3178493)

## Docs
[HPVM IR Specification](/hpvm/docs/hpvm-specification.md)

[HPVM-C Language Specification](/hpvm/docs/hpvm-c.md)

[HPVM Compilation Process](/hpvm/docs/compilation.md)

## Dependencies
The following components are required to be installed on your machine to build HPVM.

* GCC (>=5.1.0)
* CMake (>=3.4.3)
* Python (>=2.7)
* GNU Make (>=3.79.1)
* OpenCL (>=1.0.0) or CUDA (>=9.1, only required for GPU support)


## Supported Targets
Supported/tested CPU architectures:
* Intel Xeon E5-2640
* Intel Xeon W-2135
* ARM Cortex A-57

Supported/tested GPU architectures:
* Nvidia Quadro P1000
* Nvidia GeForce GTX 1080

HPVM has not been tested but might work on other CPUs supported by LLVM Backend, and GPUs supported by OpenCL such as Intel, AMD, etc.


## Getting source code and building HPVM

Checkout HPVM:
```shell
git clone https://gitlab.engr.illinois.edu/llvm/hpvm-release.git/
cd hpvm-release/hpvm
```

HPVM installer script can be used to download, configure and build HPVM along with LLVM and Clang. 
```shell
bash install.sh
```
Specifically, the HPVM installer downloads LLVM, and Clang, copies HPVM source into 
llvm/tools and builds the entire tree. It also builds a modified LLVM C-Backend, based on the one maintained by [Julia Computing](https://github.com/JuliaComputing/llvm-cbe), as a part of HPVM and is currently used 
to generate OpenCL kernels for GPUs.

In the beginning of the building process, the installer provides users the choice of automatically or manually building HPVM. 
If HPVM is selected to be built automatically, the installer allows users to type in the number of threads they want to use. 
The default number of threads used to build HPVM is two.

Alternatively, CMake can be run manually using the following steps in ./hpvm-release/hpvm directory.
```shell
mkdir build
cd build
cmake ../llvm [options]
```
Some common options that can be used with CMake are:

* -DCMAKE_INSTALL_PREFIX=directory --- Specify for directory the full pathname of where you want the HPVM tools and libraries to be installed.

* -DCMAKE_BUILD_TYPE=type --- Valid options for type are Debug, Release, RelWithDebInfo, and MinSizeRel. Default is Debug.

* -DLLVM_ENABLE_ASSERTIONS=On --- Compile with assertion checks enabled (default is Yes for Debug builds, No for all other build types).

In order to manually build and install HPVM, GNU Make can be run using the following in the build directory.
```shell
make -j<number of threads>
make install
```

In the end of the installation process, the installer automatically runs all the regression tests to ensure that the installation is
successful. If HPVM is built and installed manually, the tests can be automatically run by executing the following step from the ./hpvm-release/hpvm directory.
```shell
bash scripts/automate_tests.sh
```

With all the aforementioned steps, HPVM should be built, installed, tested and ready to use.

## Benchmarks and Tests
We are providing the following [HPVM benchmarks](/hpvm/test/benchmarks):
* Select benchmarks from the [Parboil](http://impact.crhc.illinois.edu/parboil/parboil.aspx) benchmark suite, located under [test/benchmarks/parboil](/hpvm/test/benchmarks/parboil).
* An edge detection pipeline benchmark, located under [test/benchmarks/pipeline](/hpvm/test/benchmarks/pipeline).
* A Camera ISP pipeline, located under [test/benchmarks/hpvm-cava](/hpvm/test/benchmarks/hpvm-cava), adapted from C code provided from our collaborators at [Harvard](http://vlsiarch.eecs.harvard.edu).

Benchmark descriptions and instructions on how to compile and run them are [here](/hpvm/test/benchmarks).

We are also providing [unit tests](/hpvm/test/unitTests) and [regression tests](/hpvm/test/regressionTests).

## Support
All questions can be directed to [hpvm-dev@lists.cs.illinois.edu](mailto:hpvm-dev@lists.cs.illinois.edu).


