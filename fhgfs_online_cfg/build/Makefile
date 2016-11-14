# This is the beegfs-ctl makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.

include $(or $(root-dir),../..)/build/Makefile

$(call build-executable,\
   beegfs-ctl,\
   $(shell find ../source -iname '*.cpp'),\
   opentk common client-devel cppunit dl)
