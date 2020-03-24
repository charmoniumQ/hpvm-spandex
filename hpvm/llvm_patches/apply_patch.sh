#!/bin/sh

### File Copies
cp include/IR/IntrinsicsHPVM.td  ${LLVM_SRC_ROOT}/include/llvm/IR/IntrinsicsHPVM.td


## Header File Patches
patch  ${LLVM_SRC_ROOT}/include/llvm/IR/Attributes.td  <  ./include/IR/Attributes.td.patch 

patch  ${LLVM_SRC_ROOT}/include/llvm/IR/Intrinsics.td  <  ./include/IR/Intrinsics.td.patch

patch  ${LLVM_SRC_ROOT}/include/llvm/Bitcode/LLVMBitCodes.h  <  ./include/Bitcode/LLVMBitCodes.h.patch

patch  ${LLVM_SRC_ROOT}/include/llvm/Support/Debug.h   <   ./include/Support/Debug.h.patch


#### Patching Sources 


patch  ${LLVM_SRC_ROOT}/lib/AsmParser/LLLexer.cpp   <  ./lib/AsmParser/LLLexer.cpp.patch 

patch  ${LLVM_SRC_ROOT}/lib/AsmParser/LLLexer.h   <  ./lib/AsmParser/LLLexer.h.patch

patch  ${LLVM_SRC_ROOT}/lib/AsmParser/LLParser.cpp   <   ./lib/AsmParser/LLParser.cpp.patch

patch  ${LLVM_SRC_ROOT}/lib/AsmParser/LLParser.h   <   ./lib/AsmParser/LLParser.h.patch

patch  ${LLVM_SRC_ROOT}/lib/AsmParser/LLToken.h   <   ./lib/AsmParser/LLToken.h.patch

patch  ${LLVM_SRC_ROOT}/lib/IR/Attributes.cpp  <  ./lib/IR/Attributes.cpp.patch

patch  ${LLVM_SRC_ROOT}/lib/Bitcode/Reader/BitcodeReader.cpp   <   ./lib/Bitcode/Reader/BitcodeReader.cpp.patch

patch  ${LLVM_SRC_ROOT}/lib/Bitcode/Writer/BitcodeWriter.cpp  <   ./lib/Bitcode/Writer/BitcodeWriter.cpp.patch
