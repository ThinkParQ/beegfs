include ../../build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Ctl,\
   $(sources),\
   common client-devel dl nl3-route,\
   ../source)

$(call define-dep-lib,\
   Ctl,\
   -I ../source,\
   $(build_dir)/libCtl.a)

$(call build-executable,\
   beegfs-ctl,\
   $(main),\
   Ctl common client-devel dl nl3-route,\
   ../source)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Ctl common client-devel dl nl3-route,\
   ../tests)
