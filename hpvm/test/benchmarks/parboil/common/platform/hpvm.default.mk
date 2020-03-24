# (c) 2007 The Board of Trustees of the University of Illinois.

# Rules common to all makefiles

# Commands to build objects from source file using C compiler
# with gcc

# Uncomment below two lines and configure if you want to use a platform
# other than global one

#OPENCL_PATH=/scr/hskim/ati-stream-sdk-v2.3-lnx64
#OPENCL_LIB_PATH=$(OPENCL_PATH)/lib/x86_64

CC = $(HPVM_BUILD_DIR)/bin/clang
OCLBE = $(HPVM_BUILD_DIR)/bin/llvm-cbe
PLATFORM_CFLAGS = -I$(LLVM_SRC_ROOT)/include -I$(HPVM_BUILD_DIR)/include -I../../../include

CXX = $(HPVM_BUILD_DIR)/bin/clang++
PLATFORM_CXXFLAGS = -I$(LLVM_SRC_ROOT)/include -I$(HPVM_BUILD_DIR)/include -I../../../include

LINKER = $(HPVM_BUILD_DIR)/bin/clang++
PLATFORM_LDFLAGS = -lm -lpthread -lOpenCL

LLVM_LIB_PATH = $(HPVM_BUILD_DIR)/lib
LLVM_BIN_PATH = $(HPVM_BUILD_DIR)/bin

OPT = $(LLVM_BIN_PATH)/opt
LLVM_LINK = $(LLVM_BIN_PATH)/llvm-link
LLVM_AS = $(LLVM_BIN_PATH)/llvm-as
LIT = $(LLVM_BIN_PATH)/llvm-lit

