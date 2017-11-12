# This is the beegfs_storage makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Storage,\
   $(sources),\
   opentk common cppunit dl tests,\
   ../source)

$(call define-dep-lib,\
   Storage,\
   -I ../source,\
   $(build_dir)/libStorage.a)

$(call build-executable,\
   beegfs-storage,\
   $(main),\
   Storage opentk common cppunit dl tests)

$(call build-executable,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Storage opentk common cppunit dl tests,\
   ../tests)
