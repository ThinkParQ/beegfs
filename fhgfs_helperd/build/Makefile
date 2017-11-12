# This is the beegfs_helperd makefile.
# It creates an ELF executable.
#
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Helperd,\
   $(sources),\
   opentk common cppunit dl tests,\
   ../source)

$(call define-dep-lib,\
   Helperd,\
   -I ../source,\
   $(build_dir)/libHelperd.a)

$(call build-executable,\
   beegfs-helperd,\
   $(main),\
   Helperd opentk common cppunit dl tests)

$(call build-executable,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Helperd opentk common cppunit dl tests)
