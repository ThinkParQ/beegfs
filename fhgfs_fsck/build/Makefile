# This is the beegfs-fsck makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.

include $(or $(root-dir),../..)/build/Makefile

$(call build-executable,\
   beegfs-fsck,\
   $(shell find ../source -iname '*.cpp'),\
   opentk common cppunit dl)

CXXFLAGS += -DBOOST_RESULT_OF_USE_DECLTYPE
