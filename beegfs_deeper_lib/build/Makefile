# This is the beegfs_deeper_lib makefile.
# It creates a shared library.
# 
# Use "make help" to find out about configuration options.

CAN_PACKAGE := yes

include $(or $(root-dir),../..)/build/Makefile

# rhel5 does not have useful pkgconfig information for zlib
$(call define-dep-lib, z, -I/usr/include, , -lz)

LIB = libbeegfs-deeper.so

PREFIX                ?= /opt/beegfs
ETC_DIR               ?= /etc


CXXFLAGS += -I../include

$(call build-shared-library,\
	beegfs-deeper,\
	$(shell find ../source -name '*.cpp') $(shell find ../source -name '*.c'),\
	opentk-static common deeper-common dl z cppunit-from-thirdparty,\
	../source)

package_lib: clean_package
	$(MAKE) -C $(BEEGFS_DEEPER_COMMON_PATH)/build package-lib
	$(MAKE) -C $(BEEGFS_DEEPER_CACHED_PATH)/build all
	$(MAKE)

clean_package:
	$(MAKE) -C $(BEEGFS_DEEPER_COMMON_PATH)/build clean-package-lib
	$(MAKE) -C $(BEEGFS_DEEPER_CACHED_PATH)/build clean
	$(MAKE) clean

install: build_lib
	install -D -m 755 $(LIB) $(DESTDIR)/$(PREFIX)/lib/$(LIB)

package_install: install
	@# PREFIX is something like /usr or /usr/local or /opt/beegfs
	@# DESTDIR is mainly for package builders
	install -D -m 644 dist/etc/ld.so.conf.d/beegfs-deeper.conf.in $(DESTDIR)/$(ETC_DIR)/ld.so.conf.d/beegfs-deeper.conf
	install -D -m 644 dist/etc/beegfs/beegfs-deeper-lib.conf.in $(DESTDIR)/$(ETC_DIR)/beegfs/beegfs-deeper-lib.conf
	sed -i -e "s#__PREFIX__#$(PREFIX)#" $(DESTDIR)/$(ETC_DIR)/ld.so.conf.d/beegfs-deeper.conf
	mkdir -p ${RPM_BUILD_ROOT}/usr/include
	cp -a ../include/deeper ${RPM_BUILD_ROOT}/usr/include/

define HELP_ARGS_SPECIFIC
	@echo '  PREFIX=/some/path          Installation root directory'
	@echo '                             (default: /opt/beegfs)'
	@echo '  ETC_DIR=<path>             Prefix for configuration files'
	@echo '                             (default: /etc)'
	@echo '  DESTDIR=/some/path         Mainly for package building, used as primary'
	@echo '                             path prefix then.'
endef

define HELP_TARGETS_SPECIFIC
	@echo '  install           install the library and config files.'
endef
