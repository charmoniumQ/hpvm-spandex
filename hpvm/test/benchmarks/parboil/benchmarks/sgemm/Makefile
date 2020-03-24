PARBOIL_ROOT = ../../
APP = sgemm

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
DATASET_DIR ?= $(PARBOIL_ROOT)/datasets/$(APP)

MATRIX1 = $(DATASET_DIR)/$(TEST)/input/matrix1.txt
MATRIX2 = $(DATASET_DIR)/$(TEST)/input/matrix2.txt
MATRIX2T = $(DATASET_DIR)/$(TEST)/input/matrix2t.txt
REF_OUTPUT = $(DATASET_DIR)/$(TEST)/output/matrix3.txt
RUNDIR = run/$(VERSION)/$(TEST)
OUTPUT = $(RUNDIR)/matrix3.txt

ARGS = -i $(MATRIX1),$(MATRIX2),$(MATRIX2T) -o $(OUTPUT)
TOOL = tools/compare-output
#TOOL=echo
include $(PARBOIL_ROOT)/common/mk/Makefile
