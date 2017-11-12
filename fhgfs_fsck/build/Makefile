# This is the beegfs-fsck makefile.
# It creates an ELF executable.
#
# Use "make help" to find out about configuration options.

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Fsck,\
   $(sources),\
   opentk common cppunit dl tests,\
   ../source)

$(call define-dep-lib,\
   Fsck,\
   -I ../source,\
   $(build_dir)/libFsck.a)

$(call build-executable,\
   beegfs-fsck,\
   $(main),\
   Fsck opentk common cppunit dl tests)

$(call build-executable,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Fsck opentk common cppunit dl tests,\
   ../tests)

CXXFLAGS += -DBOOST_RESULT_OF_USE_DECLTYPE
