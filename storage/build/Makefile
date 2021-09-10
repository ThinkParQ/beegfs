include ../../build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Storage,\
   $(sources),\
   common dl blkid uuid,\
   ../source)

$(call define-dep-lib,\
   Storage,\
   -I ../source,\
   $(build_dir)/libStorage.a)

$(call build-executable,\
   beegfs-storage,\
   $(main),\
   Storage common dl blkid uuid)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Storage common dl blkid uuid,\
   ../tests)
