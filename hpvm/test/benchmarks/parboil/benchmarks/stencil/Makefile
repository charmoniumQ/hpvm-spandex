PARBOIL_ROOT = ../../
APP = stencil

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
  INPUT = $(DATASET_DIR)/small/input/128x128x32.bin 128 128 32 100
  REF_OUTPUT = $(DATASET_DIR)/small/output/128x128x32.out
  RUNDIR = run/$(VERSION)/small
  OUTPUT = $(RUNDIR)/128x128x32.out
else ifeq ($(TEST),large)
  INPUT = $(DATASET_DIR)/large/input/1024x1024x128.bin 1024 1024 128 100
  REF_OUTPUT = $(DATASET_DIR)/large/output/1024x1024x128.out
  RUNDIR = run/$(VERSION)/large
  OUTPUT = $(RUNDIR)/1024x1024x128.out
else
  INPUT = $(DATASET_DIR)/default/input/512x512x64x100.bin 512 512 64 100
  REF_OUTPUT = $(DATASET_DIR)/default/output/512x512x64.out
  RUNDIR = run/$(VERSION)/default
  OUTPUT = $(RUNDIR)/512x512x64.out
endif

ARGS = -i $(INPUT) -o $(OUTPUT)
TOOL = tools/compare-output
#TOOL=echo
include $(PARBOIL_ROOT)/common/mk/Makefile
