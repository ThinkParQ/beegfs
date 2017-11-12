# This is the beegfs_deeper_common makefile.
# It creates a static library.
#
# Use "make help" to find out about configuration options.


include $(or $(root-dir),../..)/build/Makefile

# rhel5 does not have useful pkgconfig information for zlib
$(call define-dep-lib, z, -I/usr/include, , -lz)

LIB = beegfs-deeper-common.a
CXXFLAGS +=-fPIC -I$(BEEGFS_DEEPER_LIB_INCLUDE_PATH)

$(call build-static-library,\
   beegfs-deeper-common,\
   $(shell find ../source -iname '*.cpp'),\
   opentk-static common dl z cppunit-from-thirdparty,\
   ../source)

all-cppunit: cppunit dep-cppunit
	$(MAKE)

package-lib: dep
	$(MAKE)

# ignore cppunit warnings for jenkins
cppunit:
	$(MAKE) CXXFLAGS='-fPIC' -C $(BEEGFS_THIRDPARTY_PATH)/build/ clean_cppunit 2>&1
	$(MAKE) CXXFLAGS='-fPIC' -C $(BEEGFS_THIRDPARTY_PATH)/build/ cppunit 2>&1

dep-cppunit: cppunit
	$(MAKE) -C $(BEEGFS_OPENTK_PATH)/build/ USER_CXXFLAGS=-fPIC BEEGFS_OPENTK_IBVERBS=0 all
	$(MAKE) -C $(BEEGFS_COMMON_PATH)/build/ USER_CXXFLAGS=-fPIC all

dep:
	$(MAKE) -C $(BEEGFS_OPENTK_PATH)/build/ USER_CXXFLAGS=-fPIC BEEGFS_OPENTK_IBVERBS=0 all
	$(MAKE) -C $(BEEGFS_COMMON_PATH)/build/ USER_CXXFLAGS=-fPIC all


clean-dep-cppunit: clean-package-lib clean-cppunit

clean-package-lib: clean clean-dep

clean-dep:
	$(MAKE) -C $(BEEGFS_COMMON_PATH)/build/ clean
	$(MAKE) -C $(BEEGFS_OPENTK_PATH)/build/ clean

clean-cppunit:
	$(MAKE) -C $(BEEGFS_THIRDPARTY_PATH)/build/ clean_cppunit
