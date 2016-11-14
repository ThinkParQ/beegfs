# This is the beegfs_storage makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

$(call build-executable,\
   beegfs-storage,\
   $(shell find ../source -iname '*.cpp'),\
   opentk common cppunit dl)
