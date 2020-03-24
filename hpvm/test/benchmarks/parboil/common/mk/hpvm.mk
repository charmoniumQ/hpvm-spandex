# (c) 2007 The Board of Trustees of the University of Illinois.

# Default language wide options
LANG_CFLAGS=-I$(PARBOIL_ROOT)/common/include
LANG_CXXFLAGS=$(LANG_CFLAGS)  
LANG_LDFLAGS=-lOpenCL -L$(OPENCL_LIB_PATH) -lrt -L$(CUDA_LIB_PATH) -lcudart

CFLAGS=$(LANG_CFLAGS) $(PLATFORM_CFLAGS) $(APP_CFLAGS)
CXXFLAGS=$(LANG_CXXFLAGS) $(PLATFORM_CXXFLAGS) $(APP_CXXFLAGS)
LDFLAGS=$(LANG_LDFLAGS) $(PLATFORM_LDFLAGS) $(APP_LDFLAGS)

# HPVM
HPVM_RT_PATH = $(LLVM_BUILD_DIR)/tools/hpvm/projects/hpvm-rt

HPVM_RT_LIB = $(HPVM_RT_PATH)/hpvm-rt.bc

TESTGEN_OPTFLAGS = -load LLVMGenHPVM.so -genhpvm -globaldce
KERNEL_GEN_FLAGS = -O3 -target opencl64-nvidia-nvcl

ifeq ($(TARGET),seq)
  DEVICE = CPU_TARGET
  HPVM_OPTFLAGS = -load LLVMBuildDFG.so -load LLVMDFG2LLVM_CPU.so -load LLVMClearDFG.so -dfg2llvm-cpu -clearDFG
else
  DEVICE = GPU_TARGET
  HPVM_OPTFLAGS = -load LLVMBuildDFG.so -load LLVMLocalMem.so -load LLVMDFG2LLVM_OpenCL.so -load LLVMDFG2LLVM_CPU.so -load LLVMClearDFG.so -localmem -dfg2llvm-opencl -dfg2llvm-cpu -clearDFG
endif

CFLAGS += -DDEVICE=$(DEVICE)
CXXFLAGS += -DDEVICE=$(DEVICE)

HOST_LINKFLAGS =

ifeq ($(TIMER),cpu)
  HPVM_OPTFLAGS += -hpvm-timers-cpu
else ifeq ($(TIMER),gen)
  TESTGEN_OPTFLAGS += -hpvm-timers-gen
else ifeq ($(TIMER),no)
else
  ifeq ($(TARGET),seq)
    HPVM_OPTFLAGS += -hpvm-timers-cpu
  else
    HPVM_OPTFLAGS += -hpvm-timers-cpu -hpvm-timers-ptx
  endif
  TESTGEN_OPTFLAGS += -hpvm-timers-gen
endif

# Rules common to all makefiles

########################################
# Functions
########################################

# Add BUILDDIR as a prefix to each element of $1
INBUILDDIR=$(addprefix $(BUILDDIR)/,$(1))

# Add SRCDIR as a prefix to each element of $1
INSRCDIR=$(addprefix $(SRCDIR)/,$(1))

########################################
# Environment variable check
########################################

OPENCV_LIB_PATH=${OpenCV_DIR}/lib

# The second-last directory in the $(BUILDDIR) path
# must have the name "build".  This reduces the risk of terrible
# accidents if paths are not set up correctly.
ifeq ("$(notdir $(BUILDDIR))", "")
$(error $$BUILDDIR is not set correctly)
endif

ifneq ("$(notdir $(patsubst %/,%,$(dir $(BUILDDIR))))", "build")
$(error $$BUILDDIR is not set correctly)
endif

.PHONY: run
.PRECIOUS: $(BUILDDIR)/%.ll

ifeq ($(OPENCL_PATH),)
FAILSAFE=no_opencl
else 
FAILSAFE=
endif

########################################
# Derived variables
########################################

OBJS = $(call INBUILDDIR,$(SRCDIR_OBJS))
TEST_OBJS = $(call INBUILDDIR,$(HPVM_OBJS))
PARBOIL_OBJS = $(call INBUILDDIR,parboil.ll)
KERNEL = $(TEST_OBJS).kernels.ll
ifeq ($(TARGET),seq)
else
  KERNEL_OCL = $(TEST_OBJS).kernels.cl
endif
HOST_LINKED = $(BUILDDIR)/$(APP).linked.ll
HOST = $(BUILDDIR)/$(APP).host.ll

ifeq ($(DEBUGGER),)
DEBUGGER=gdb
endif

########################################
# Rules
########################################

default: $(FAILSAFE) $(BUILDDIR) $(KERNEL_OCL) $(SPIR_ASSEMBLY) $(BIN)
#default: $(FAILSAFE) $(BUILDDIR) $(BIN)

run : $(RUNDIR)
	echo "Resolving CUDA library..."
	$(shell echo $(RUNTIME_ENV)) LD_LIBRARY_PATH=$(CUDA_LIB_PATH) ldd ./$(BIN) | grep cuda
	#echo "Resolving OpenCV library..."
	#$(shell echo $(RUNTIME_ENV)) LD_LIBRARY_PATH=$(OPENCV_LIB_PATH) ldd ./$(BIN) | grep opencv
	$(shell echo $(RUNTIME_ENV)) LD_LIBRARY_PATH=$(OPENCL_LIB_PATH):$(OPENCV_LIB_PATH):$(CUDA_LIB_PATH) ./$(BIN) $(ARGS)
	$(TOOL) $(OUTPUT) $(REF_OUTPUT)

debug:
	@echo "Resolving OpenCL library..."
	@$(shell echo $(RUNTIME_ENV)) LD_LIBRARY_PATH=$(OPENCL_LIB_PATH) ldd $(BIN) | grep OpenCL
	@$(shell echo $(RUNTIME_ENV)) LD_LIBRARY_PATH=$(OPENCL_LIB_PATH) $(DEBUGGER) --args $(BIN) $(ARGS)

clean :
	rm -rf $(BUILDDIR)/*
	if [ -f $(BIN) ]; then rm $(BIN); fi
	if [ -f DataflowGraph.dot ]; then rm DataflowGraph.dot*; fi
	if [ -d $(BUILDDIR) ]; then rm -rf $(BUILDDIR); fi
	if [ -d $(RUNDIR) ]; then rm -rf $(RUNDIR); fi

$(KERNEL_OCL) : $(KERNEL)
	$(OCLBE) $< -o $@

$(BIN) : $(HOST_LINKED)
	$(CXX) -O3 $(LDFLAGS) $< -o $@

$(HOST_LINKED) : $(HOST) $(OBJS) $(BUILDDIR)/parboil.ll $(HPVM_RT_LIB)
	$(LLVM_LINK) $^ -S -o $@

$(HOST) $(KERNEL): $(BUILDDIR)/$(HPVM_OBJS)
	$(OPT) $(HPVM_OPTFLAGS) -S $< -o $(HOST)

$(RUNDIR) :
	mkdir -p $(RUNDIR)

$(BUILDDIR) :
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.ll: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -S -emit-llvm $< -o $@

$(BUILDDIR)/%.ll : $(SRCDIR)/%.cc
	$(CXX) $(CXXFLAGS) -S -emit-llvm $< -o $@

$(BUILDDIR)/%.ll : $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -S -emit-llvm $< -o $@

$(BUILDDIR)/%.hpvm.ll: $(BUILDDIR)/%.ll
	$(OPT) $(TESTGEN_OPTFLAGS) $< -S -o $@

$(BUILDDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/parboil.ll : $(PARBOIL_ROOT)/common/src/parboil.c
	$(CC) $(CFLAGS) -S -emit-llvm $< -o $@

$(BUILDDIR)/%.o : $(SRCDIR)/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

no_opencl:
	@echo "OPENCL_PATH is not set. Open $(PARBOIL_ROOT)/common/Makefile.conf to set default value."
	@echo "You may use $(PLATFORM_MK) if you want a platform specific configurations."
	@exit 1

