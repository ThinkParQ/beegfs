include ../../build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Mgmtd,\
   $(sources),\
   common dl nl3-route,\
   ../source)

$(call define-dep-lib,\
   Mgmtd,\
   -I ../source,\
   $(build_dir)/libMgmtd.a)

$(call build-executable,\
   beegfs-mgmtd,\
   $(main),\
   Mgmtd common dl nl3-route)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Mgmtd common dl nl3-route,\
   ../tests)
