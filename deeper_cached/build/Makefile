include $(or $(root-dir),../..)/build/Makefile

CXXFLAGS += -I$(BEEGFS_DEEPER_LIB_INCLUDE_PATH)

$(call build-executable,\
   beegfs-deeper-cached,\
   $(shell find ../source -iname '*.cpp'),\
   opentk-static common deeper-common dl z,\
   ../source)
