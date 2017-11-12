# This is the beegfs_mon makefile.
# It creates an ELF executable.
#
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/app/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Mon,\
   $(sources),\
   common opentk cppunit dl tests curl,\
   ../source)

$(call define-dep-lib,\
   Mon,\
   -I ../source,\
   $(build_dir)/libMon.a)

$(call build-executable,\
   beegfs-mon,\
   $(main),\
   Mon common opentk cppunit dl tests curl)

$(call build-executable,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Mon common opentk cppunit dl tests curl,\
   ../tests)

