CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Helperd,\
   $(sources),\
   opentk common dl,\
   ../source)

$(call define-dep-lib,\
   Helperd,\
   -I ../source,\
   $(build_dir)/libHelperd.a)

$(call build-executable,\
   beegfs-helperd,\
   $(main),\
   Helperd opentk common dl)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Helperd opentk common dl)
