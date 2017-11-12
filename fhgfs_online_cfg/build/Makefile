# This is the beegfs-ctl makefile.
# It creates an ELF executable.
#
# Use "make help" to find out about configuration options.

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Ctl,\
   $(sources),\
   opentk common client-devel cppunit tests dl,\
   ../source)

$(call define-dep-lib,\
   Ctl,\
   -I ../source,\
   $(build_dir)/libCtl.a)

$(call build-executable,\
   beegfs-ctl,\
   $(main),\
   Ctl opentk common client-devel cppunit dl tests,\
   ../source)
