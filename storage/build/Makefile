CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Storage,\
   $(sources),\
   opentk common dl,\
   ../source)

$(call define-dep-lib,\
   Storage,\
   -I ../source,\
   $(build_dir)/libStorage.a)

$(call build-executable,\
   beegfs-storage,\
   $(main),\
   Storage opentk common dl)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Storage opentk common dl,\
   ../tests)
