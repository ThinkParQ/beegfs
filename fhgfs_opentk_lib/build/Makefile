# This is the beegfs_opentk makefile.
# It creates a shared library.
# 
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

LIB       = libbeegfs-opentk.so
LIB_NO_IB = libbeegfs-opentk-disabledIB.so
LIB_IB    = libbeegfs-opentk-enabledIB.so

# Include System Feature Auto-Detectors
include SystemFeatureDetection.mk

CXXFLAGS += $(SYSTEM_FEATURE_DETECTION) -I ../include -fPIC
CXXFLAGS_DEBUG += -DBEEGFS_OPENTK_LOG_CONN_ERRORS

SOURCES = $(shell find ../source -name '*.cpp')

$(call build-static-library,\
   beegfs-opentk,\
   $(SOURCES))

$(call build-shared-library,\
   beegfs-opentk,\
   $(SOURCES))

PREFIX                ?= /opt/beegfs
ETC_DIR               ?= /etc

SRC_PATH=$(DESTDIR)/$(PREFIX)/src/beegfs_opentk_lib


# OFED
ifeq ($(BEEGFS_OPENTK_IBVERBS),1)
CXXFLAGS += -DBEEGFS_OPENTK_IBVERBS
LDFLAGS += -lrdmacm -libverbs
endif

ifneq ($(OFED_INCLUDE_PATH),)
CXXFLAGS += -I$(OFED_INCLUDE_PATH)
endif

ifneq ($(OFED_LIB_PATH),)
LDFLAGS += -L$(OFED_LIB_PATH)
endif


# build without and with IB support
package_lib:
	# clean up old stuff first
	$(MAKE) clean

	# with IB first
	$(MAKE) BEEGFS_OPENTK_IBVERBS=1
	mv $(LIB) $(LIB_IB).save

	$(MAKE) clean

	# now without IB
	$(MAKE) BEEGFS_OPENTK_IBVERBS=0

	mv $(LIB) $(LIB_NO_IB)
	mv $(LIB_IB).save $(LIB_IB)

	$(STRIP) --strip-debug $(LIB_NO_IB)
	$(STRIP) --strip-debug $(LIB_IB)

install: package_lib
	install -D -m 644 $(LIB_NO_IB) $(DESTDIR)/$(PREFIX)/lib/$(LIB_NO_IB)
	install -D -m 644 $(LIB_IB)    $(DESTDIR)/$(PREFIX)/lib/$(LIB_IB)

package_install: install
	@# PREFIX is something like /usr or /usr/local or /opt/beegfs
	@# DESTDIR is mainly for package builders
	install -D -m 644 dist/etc/ld.so.conf.d/beegfs.conf.in $(DESTDIR)/$(ETC_DIR)/ld.so.conf.d/beegfs.conf
	install -D -m 644 dist/etc/beegfs/beegfs-libopentk.conf $(DESTDIR)/$(ETC_DIR)/beegfs/beegfs-libopentk.conf
	sed -i -e "s#__PREFIX__#$(PREFIX)#" $(DESTDIR)/$(ETC_DIR)/ld.so.conf.d/beegfs.conf

	install -D ../scripts/beegfs-setup-rdma \
		$(DESTDIR)/usr/sbin/beegfs-setup-rdma

	@ echo "Copying opentk sources ..." 
	mkdir --parents ${SRC_PATH}/build
	cp Makefile $(SRC_PATH)/build/
	cp SystemFeatureDetection.mk $(SRC_PATH)/build/
	cp -a dist $(SRC_PATH)/build/

	find ../include -mount -name '*.h' -type f | \
		xargs -I '{}' cp --parents '{}' $(SRC_PATH)/build
	find ../source -mount -name '*.h' -type f | \
		xargs -I '{}' cp --parents '{}' $(SRC_PATH)/build
	find ../source -mount -name '*.c' -type f | \
		xargs -I '{}' cp --parents '{}' $(SRC_PATH)/build
	find ../source -mount -name '*.cpp' -type f | \
		xargs -I '{}' cp --parents '{}' $(SRC_PATH)/build

CLEANUP_FILES += $(LIB) $(LIB_NO_IB) $(LIB_IB)


define HELP_ARGS_SPECIFIC
	@echo '  BEEGFS_OPENTK_IBVERBS=1    Defining this enables ibverbs support'
	@echo '  OFED_INCLUDE_PATH=<path>   Path to OpenFabrics Enterpise Distribution'
	@echo '                             include directory'
	@echo '                             Note: This directory contains e.g. "rdma/rdma_cma.h"'
	@echo '                             (If this is undefined then standard system include'
	@echo '                             paths will be used.)'
	@echo '  OFED_LIB_PATH=<path>       Path to OpenFabrics Enterpise Distribution lib directory'
	@echo '                             Note: This directory contains librdmacm and libibverbs'
	@echo '  PREFIX=/some/path          Installation root directory (default: /opt/beegfs)'
	@echo '  ETC_DIR                    Prefix for configuration files (default: /etc)'
	@echo '  DESTDIR=/some/path         Mainly for package building, used as primary path prefix then'
endef
