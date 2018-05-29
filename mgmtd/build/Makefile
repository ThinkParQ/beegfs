CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Mgmtd,\
   $(sources),\
   opentk common dl,\
   ../source)

$(call define-dep-lib,\
   Mgmtd,\
   -I ../source,\
   $(build_dir)/libMgmtd.a)

$(call build-executable,\
   beegfs-mgmtd,\
   $(main),\
   Mgmtd opentk common dl)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Mgmtd opentk common dl,\
   ../tests)
