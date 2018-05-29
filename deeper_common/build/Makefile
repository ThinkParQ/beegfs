include $(or $(root-dir),../..)/build/Makefile

LIB = beegfs-deeper-common.a
CXXFLAGS +=-fPIC -I$(BEEGFS_DEEPER_LIB_INCLUDE_PATH)

$(call build-static-library,\
   beegfs-deeper-common,\
   $(shell find ../source -iname '*.cpp'),\
   opentk-static common dl z,\
   ../source)

all: dep

package-lib: dep
	$(MAKE)

dep:
	$(MAKE) -C $(BEEGFS_OPENTK_PATH)/build/ USER_CXXFLAGS=-fPIC BEEGFS_OPENTK_IBVERBS=0 all
	$(MAKE) -C $(BEEGFS_COMMON_PATH)/build/ USER_CXXFLAGS=-fPIC all


clean-package-lib: clean clean-dep

clean-dep:
	$(MAKE) -C $(BEEGFS_COMMON_PATH)/build/ clean
	$(MAKE) -C $(BEEGFS_OPENTK_PATH)/build/ clean
