# Using HPVM
The below benchmarks are provided with HPVM, along with a template Makefile for user projects. 

In order to be able to build the existing benchmarks, a new `Makefile.config` must be created in [include](/hpvm/test/benchmarks/include) based on the existing `Makefile.config.example`. This configuration file must set up the following paths:
* LLVM_BUILD_DIR: should point to your local `build` directory of HPVM.
* HPVM_BENCH_DIR: should point to this benchmakrs directory.
* CUDA_PATH: should point to your local CUDA installation.

## Parboil
Instructions to compile and run Parboil are provided in the following [README](/hpvm/test/benchmarks/parboil).

## Harvard Camera Pipeline (HPVM-CAVA)
Instructions to compile and run HPVM-CAVA are provided in the following [README](/hpvm/test/benchmarks/hpvm-cava).

## Edge Detection Pipeline
Instructions to compile and run Pipeline are provided in the following [README](/hpvm/test/benchmarks/pipeline).

## Your own project
See `template/` for an example Makefile and config.
Include `hpvm.h` to use HPVM C api functions, found in the `include/hpvm.h`.
