PARBOIL_ROOT = ../../
APP = lbm

ifeq ($(NUM_CORES),)
  NUM_CORES=8
endif

# Default compile hpvm
ifeq ($(VERSION),)
  VERSION = hpvm
endif

# Default use small test case
ifeq ($(TEST),)
  TEST = short
endif

ifeq ($(PLATFORM),)
PLATFORM=default
endif

BIN = $(addsuffix -$(VERSION), $(APP))

SRCDIR = src/$(VERSION)
BUILDDIR = build/$(VERSION)_$(PLATFORM)
DATASET_DIR = $(PARBOIL_ROOT)/datasets/$(APP)

ifeq ($(TEST),long)
  INPUT = $(DATASET_DIR)/long/input/120_120_150_ldc.of 3000
  REF_OUTPUT = $(DATASET_DIR)/long/output/reference.dat
  RUNDIR = run/$(VERSION)/long
  OUTPUT = $(RUNDIR)/reference.dat
else
  INPUT = $(DATASET_DIR)/short/input/120_120_150_ldc.of 100
  REF_OUTPUT = $(DATASET_DIR)/short/output/reference.dat
  RUNDIR = run/$(VERSION)/short
  OUTPUT = $(RUNDIR)/reference.dat
endif

ARGS = -i $(INPUT) -o $(OUTPUT)
TOOL = tools/compare-output
#TOOL=echo
include $(PARBOIL_ROOT)/common/mk/Makefile

