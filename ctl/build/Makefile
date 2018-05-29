include $(or $(root-dir),../..)/build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Ctl,\
   $(sources),\
   opentk common client-devel dl,\
   ../source)

$(call define-dep-lib,\
   Ctl,\
   -I ../source,\
   $(build_dir)/libCtl.a)

$(call build-executable,\
   beegfs-ctl,\
   $(main),\
   Ctl opentk common client-devel dl,\
   ../source)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Ctl opentk common client-devel dl,\
   ../tests)
