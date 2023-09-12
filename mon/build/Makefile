include ../../build/Makefile

main := ../source/app/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Mon,\
   $(sources),\
   common dl curl cassandra nl3-route,\
   ../source)

$(call define-dep-lib,\
   Mon,\
   -I ../source,\
   $(build_dir)/libMon.a)

$(call build-executable,\
   beegfs-mon,\
   $(main),\
   Mon common dl curl cassandra nl3-route)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Mon common dl curl cassandra nl3-route,\
   ../tests)

