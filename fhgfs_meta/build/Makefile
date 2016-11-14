# This is the beegfs_meta makefile.
# It creates an ELF executable.
# 
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

$(call build-executable,\
   beegfs-meta,\
   $(shell find ../source -iname '*.cpp'),\
   opentk common cppunit dl)

# enable special reference DirInode debug code
ifneq ($(BEEGFS_DEBUG_RELEASE_DIR),)     # extra release dir debugging
   CXXFLAGS += -DBEEGFS_DEBUG_RELEASE_DIR
endif
