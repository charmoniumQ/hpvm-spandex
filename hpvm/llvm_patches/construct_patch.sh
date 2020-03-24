#!/bin/sh

#### Computing Header Diff
diff -u  $LLVM_SRC_ROOT/include/llvm/Bitcode/LLVMBitCodes.h  include/Bitcode/LLVMBitCodes.h > include/Bitcode/LLVMBitCodes.h.patch 

diff -u  $LLVM_SRC_ROOT/include/llvm/IR/Attributes.td   include/IR/Attributes.td   > include/IR/Attributes.td.patch

diff -u  $LLVM_SRC_ROOT/include/llvm/IR/Intrinsics.td   include/IR/Intrinsics.td > include/IR/Intrinsics.td.patch

diff -u  $LLVM_SRC_ROOT/include/llvm/Support/Debug.h   include/Support/Debug.h > include/Support/Debug.h.patch


#### Computing Source File Diff

diff -u  $LLVM_SRC_ROOT/lib/AsmParser/LLLexer.cpp   lib/AsmParser/LLLexer.cpp > lib/AsmParser/LLLexer.cpp.patch 

diff -u  $LLVM_SRC_ROOT/lib/AsmParser/LLLexer.h   lib/AsmParser/LLLexer.h > lib/AsmParser/LLLexer.h.patch

diff -u  $LLVM_SRC_ROOT/lib/AsmParser/LLParser.cpp   lib/AsmParser/LLParser.cpp > lib/AsmParser/LLParser.cpp.patch

diff -u  $LLVM_SRC_ROOT/lib/AsmParser/LLParser.h   lib/AsmParser/LLParser.h > lib/AsmParser/LLParser.h.patch

diff -u  $LLVM_SRC_ROOT/lib/AsmParser/LLToken.h   lib/AsmParser/LLToken.h > lib/AsmParser/LLToken.h.patch

diff -u  $LLVM_SRC_ROOT/lib/IR/Attributes.cpp   lib/IR/Attributes.cpp > lib/IR/Attributes.cpp.patch

diff -u  $LLVM_SRC_ROOT/lib/Bitcode/Reader/BitcodeReader.cpp   lib/Bitcode/Reader/BitcodeReader.cpp > lib/Bitcode/Reader/BitcodeReader.cpp.patch

diff -u  $LLVM_SRC_ROOT/lib/Bitcode/Writer/BitcodeWriter.cpp   lib/Bitcode/Writer/BitcodeWriter.cpp > lib/Bitcode/Writer/BitcodeWriter.cpp.patch
