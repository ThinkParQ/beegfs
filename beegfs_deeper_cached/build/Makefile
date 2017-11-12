# This is the beegfs_deeper_cached makefile.
# It creates an ELF executable.
#
# Use "make help" to find out about configuration options.

include $(or $(root-dir),../..)/build/Makefile

CXXFLAGS += -I$(BEEGFS_DEEPER_LIB_INCLUDE_PATH)

# rhel5 does not have useful pkgconfig information for zlib
$(call define-dep-lib, z, -I/usr/include, , -lz)

$(call build-executable,\
   beegfs-deeper-cached,\
   $(shell find ../source -iname '*.cpp'),\
   opentk-static common deeper-common cppunit-from-thirdparty dl z,\
   ../source)
