# (c) 2010 The Board of Trustees of the University of Illinois.

LANGUAGE=hpvm
TOOLS_SRC=common_src/convert-dataset
SRCDIR_OBJS=gpu_info.ll file.ll
HPVM_OBJS=main.hpvm.ll
APP_CUDALDFLAGS=-lm
APP_CFLAGS=-ffast-math -O1 -I$(TOOLS_SRC)
APP_CXXFLAGS=-ffast-math -O1 -I$(TOOLS_SRC)

include $(TOOLS_SRC)/commontools.mk
