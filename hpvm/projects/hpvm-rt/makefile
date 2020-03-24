#LLVM_SRC_ROOT = 
LLVM_BUILD_ROOT = ${LLVM_SRC_ROOT}/../build/

CUDA_INC_PATH = /software/cuda-9.1/include/CL/


ifeq ($(NUM_CORES),)
  NUM_CORES=1
endif

CPP_FLAGS = -I$(LLVM_SRC_ROOT)/include -I$(LLVM_BUILD_ROOT)/include -I$(CUDA_INC_PATH) -std=c++11 -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS
TARGET:=hpvm-rt

LLVM_CC:=$(LLVM_BUILD_ROOT)/bin/clang
LLVM_CXX:=$(LLVM_BUILD_ROOT)/bin/clang++

OPTS =

ifeq ($(DEBUG),1)
  OPTS+=-DDEBUG_BUILD
endif

all: $(TARGET:%=%.ll)

$(TARGET:%=%.ll):%.ll:%.cpp %.h
	$(LLVM_CXX) -DNUM_CORES=$(NUM_CORES) -O3 -S -emit-llvm $(CPP_FLAGS) $(OPTS) $< -o $@

clean :
	rm -f $(TARGET).ll
