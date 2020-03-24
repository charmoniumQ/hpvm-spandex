PARBOIL_ROOT = ../../
APP = spmv

# Default compile hpvm
ifeq ($(VERSION),)
  VERSION = hpvm
endif

# Default use small test case
ifeq ($(TEST),)
  TEST = small
endif

ifeq ($(PLATFORM),)
PLATFORM=default
endif


BIN = $(addsuffix -$(VERSION), $(APP))

SRCDIR = src/$(VERSION)
BUILDDIR = build/$(VERSION)_$(PLATFORM)
DATASET_DIR = $(PARBOIL_ROOT)/datasets/$(APP)

ifeq ($(TEST),small)
  INPUT1 = $(DATASET_DIR)/small/input/1138_bus.mtx
  INPUT2 = $(DATASET_DIR)/small/input/vector.bin
  REF_OUTPUT = $(DATASET_DIR)/small/output/1138_bus.mtx.out
  RUNDIR = run/$(VERSION)/small
  OUTPUT = $(RUNDIR)/1138_bus.mtx.out
else ifeq ($(TEST),medium)
  INPUT1 = $(DATASET_DIR)/medium/input/bcsstk18.mtx
  INPUT2 = $(DATASET_DIR)/medium/input/vector.bin
  REF_OUTPUT = $(DATASET_DIR)/medium/output/bcsstk18.mtx.out
  RUNDIR = run/$(VERSION)/medium
  OUTPUT = $(RUNDIR)/bcsstk18.mtx.out
else ifeq ($(TEST),huge)
  INPUT1 = $(DATASET_DIR)/huge/input/ldoor.mtx
  INPUT2 = $(DATASET_DIR)/huge/input/vector.bin
  REF_OUTPUT = $(DATASET_DIR)/huge/output/ldoor.mtx.out
  RUNDIR = run/$(VERSION)/huge
  OUTPUT = $(RUNDIR)/ldoor.mtx.out
else ifeq ($(TEST),5)
  INPUT1 = $(DATASET_DIR)/5/input/nlpkkt160.mtx
  INPUT2 = $(DATASET_DIR)/5/input/vector.bin
  REF_OUTPUT = $(DATASET_DIR)/5/output/nlpkkt160.mtx.out
  RUNDIR = run/$(VERSION)/5
  OUTPUT = $(RUNDIR)/nlpkkt160.mtx.out

else
  INPUT1 = $(DATASET_DIR)/large/input/Dubcova3.mtx.bin
  INPUT2 = $(DATASET_DIR)/large/input/vector.bin
  REF_OUTPUT = $(DATASET_DIR)/large/output/Dubcova3.mtx.out
  RUNDIR = run/$(VERSION)/large
  OUTPUT = $(RUNDIR)/Dubcova3.mtx.out
endif

ARGS = -i $(INPUT1),$(INPUT2) -o $(OUTPUT)
TOOL = tools/compare-output
#TOOL=echo
include $(PARBOIL_ROOT)/common/mk/Makefile
