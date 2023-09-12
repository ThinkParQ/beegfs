include ../../build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Helperd,\
   $(sources),\
   common dl nl3-route,\
   ../source)

$(call define-dep-lib,\
   Helperd,\
   -I ../source,\
   $(build_dir)/libHelperd.a)

$(call build-executable,\
   beegfs-helperd,\
   $(main),\
   Helperd common dl nl3-route)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Helperd common dl nl3-route)
