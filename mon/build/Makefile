CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/app/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Mon,\
   $(sources),\
   common opentk dl curl,\
   ../source)

$(call define-dep-lib,\
   Mon,\
   -I ../source,\
   $(build_dir)/libMon.a)

$(call build-executable,\
   beegfs-mon,\
   $(main),\
   Mon common opentk dl curl)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Mon common opentk dl curl,\
   ../tests)

