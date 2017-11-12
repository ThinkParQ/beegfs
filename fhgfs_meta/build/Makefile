# This is the beegfs_meta makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Meta,\
   $(sources),\
   opentk common cppunit tests dl,\
   ../source)

$(call define-dep-lib,\
   Meta,\
   -I ../source,\
   $(build_dir)/libMeta.a)

$(call build-executable,\
   beegfs-meta,\
   $(main),\
   Meta opentk common cppunit dl tests)

$(call build-executable,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Meta common opentk cppunit dl tests,\
   ../tests)

# enable special reference DirInode debug code
ifneq ($(BEEGFS_DEBUG_RELEASE_DIR),)     # extra release dir debugging
   CXXFLAGS += -DBEEGFS_DEBUG_RELEASE_DIR
endif
