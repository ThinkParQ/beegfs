# This is the beegfs_common makefile.
# It creates a static library.
# 
# Use "make help" to find out about configuration options.

include $(or $(root-dir),../..)/build/Makefile

CXXFLAGS += -I $(BEEGFS_OPENTK_PATH)/include

$(call build-static-library,\
   beegfs-common,\
   $(shell find ../source -iname '*.cpp'),\
   cppunit)

$(call build-executable,\
   test-runner,\
   $(shell find ../tests -name '*.cpp'),\
   common cppunit)

test-runner: libbeegfs-common.a
