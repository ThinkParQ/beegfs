# This is the beegfs_mgmtd makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Mgmtd,\
   $(sources),\
   opentk common cppunit dl tests,\
   ../source)

$(call define-dep-lib,\
   Mgmtd,\
   -I ../source,\
   $(build_dir)/libMgmtd.a)

$(call build-executable,\
   beegfs-mgmtd,\
   $(main),\
   Mgmtd opentk common cppunit dl tests)

$(call build-executable,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Mgmtd opentk common cppunit dl tests,\
   ../tests)
