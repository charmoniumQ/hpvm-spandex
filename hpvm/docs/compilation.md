# HPVM Compilation Process
Compilation of an HPVM program involves the following steps:

1. `clang` takes an HPVM-C/C++ program (e.g. `main.c`) and produces an LLVM IR (`main.ll`) file that contains the HPVM-C function calls. The declarations of these functions are defined in `test/benchmark/include/hpvm.h`, which must be included in the program.
2. `opt` takes (`main.ll`) and invoke the GenHPVM pass on it, which converts the HPVM-C function calls to HPVM intrinsics. This generates the HPVM textual representation (`main.hpvm.ll`).
3. `opt` takes the HPVM textual representation (`main.hpvm.ll`) and invokes the following passes in sequence: 
    * BuildDFG: Converts the textual representation to the internal HPVM representation.
    * LocalMem and DFG2LLVM_OpenCL: Invoked only when GPU target is selected. Generates the kernel module (`main.kernels.ll`) and the portion of the host code that invokes the kernel into the host module (`main.host.ll`).
    * DFG2LLVM_CPU: Generates either all, or the remainder of the host module (`main.host.ll`) depending on the chosen target.
    * ClearDFG: Deletes the internal HPVM representation from memory.
4. `clang` is used to to compile any remaining project files that would be later linked with the host module.
5. `llvm-link` takes the host module and all the other generate `ll` files, and links them with the HPVM runtime module (`hpvm-rt.bc`), to generate the linked host module (`main.host.linked.ll`). 
6. Generate the executable code from the generated `ll` files for all parts of the program:
    * GPU target: `llvm-cbe` takes the kernel module (`main.kernels.ll`) and generates an OpenCL representation of the kernels that will be invoked by the host.
    * CPU target: `clang` takes the linked  host module (`main.host.linked.ll`) and generates the CPU binary.
