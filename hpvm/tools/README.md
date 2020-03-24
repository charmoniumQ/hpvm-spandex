# hpvm-config -- Print HPVM compilation options

### Synopsis
hpvm-config option [components…]

### Description
hpvm-config print the compiler flags, linker flags and object libraries needed to link against HPVM.
In addtion to printing flags printed by [llvm-config](http://llvm.org/docs/CommandGuide/llvm-config.html), hpvm-config also prints HPVM version and compiler
flags required to compile HPVM programs.

### Options
–version: Print the version number of HPVM.

-llvm-version: Print LLVM version.

-hpvm-version: Print HPVM version.

-help: Print a summary of llvm-config arguments.

–prefix: Print the installation prefix for LLVM.

–src-root: Print the source root from which LLVM was built.

–obj-root: Print the object root used to build HPVM.

–bindir: Print the installation directory for LLVM binaries.

–includedir: Print the installation directory for LLVM headers.

–libdir: Print the installation directory for LLVM libraries.

–cxxflags: Print the C++ compiler flags needed to use LLVM headers.

–ldflags: Print the flags needed to link against LLVM libraries.

–libs: Print all the libraries needed to link against the specified LLVM components, including any dependencies.

–libnames: Similar to –libs, but prints the bare filenames of the libraries without -l or pathnames. Useful for linking against a not-yet-installed copy of LLVM.

–libfiles: Similar to –libs, but print the full path to each library file. This is useful when creating makefile dependencies, to ensure that a tool is relinked if any library it uses changes.

–components: Print all valid component names.

–targets-built: Print the component names for all targets supported by this copy of LLVM.

–build-mode: Print the build mode used when LLVM was built (e.g. Debug or Release)

-genHPVM: Generate HPVM textual IR from LLVM IR.

-dfg2llvm-cpu: Generate code for CPU and host.

-dfg2llvm-opencl: Generate kernel code for GPU target in OpenCL.

-clearDFG: Clear dataflow graph for HPVM and extraneous HPVM-specific instructions from IR.
    
### Exit Status
If hpvm-config succeeds, it will exit with 0. Otherwise, if an error occurs, it will exit with a non-zero value.




