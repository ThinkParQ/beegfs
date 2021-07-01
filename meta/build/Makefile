include ../../build/Makefile

main := ../source/program/Main.cpp
sources := $(filter-out $(main), $(shell find ../source -iname '*.cpp'))

$(call build-static-library,\
   Meta,\
   $(sources),\
   common dl blkid uuid,\
   ../source)

$(call define-dep-lib,\
   Meta,\
   -I ../source,\
   $(build_dir)/libMeta.a)

$(call build-executable,\
   beegfs-meta,\
   $(main),\
   Meta common dl blkid uuid)

$(call build-test,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   Meta common dl blkid uuid,\
   ../tests)

# enable special reference DirInode debug code
ifneq ($(BEEGFS_DEBUG_RELEASE_DIR),)     # extra release dir debugging
   CXXFLAGS += -DBEEGFS_DEBUG_RELEASE_DIR
endif
