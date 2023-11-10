# all variables used by package builds must be exported.
export BEEGFS_DEBUG
export USER_CXXFLAGS
export USER_LDFLAGS
export BEEGFS_DEBUG_RDMA
export BEEGFS_DEBUG_IP

export KDIR
export KSRCDIR

# Tell makefiles that this is a development build. Don't use config files in /etc/beegfs.
BEEGFS_DEV_BUILD=1
export BEEGFS_DEV_BUILD

BEEGFS_THIRDPARTY_OPTIONAL =
export BEEGFS_THIRDPARTY_OPTIONAL

WITHOUT_COMM_DEBUG = true

# if version is not set, derive from current git.
# debian epoch for unversioned builds (usually dev builds) is 19 because it was 18 previously.
# versioned builds (usually from tags) are set to epoch 20, allowing upgrades from all previous versions.
ifndef BEEGFS_VERSION
  BEEGFS_VERSION := $(shell git describe --tags --match '*.*' --abbrev=10)
  BEEGFS_EPOCH := 19
else
  BEEGFS_EPOCH := 20
endif

# sanitize version strings:
# rpm accepts underscores but not dashes
# deb accepts dashes but not underscores
BEEGFS_VERSION_RPM := $(subst -,_,$(BEEGFS_VERSION))
BEEGFS_VERSION_DEB := $(BEEGFS_EPOCH):$(subst _,-,$(BEEGFS_VERSION))
export BEEGFS_VERSION

ifneq ($(NVFS_INCLUDE_PATH),)
BEEGFS_NVFS=1
endif
export BEEGFS_NVFS

PREFIX ?= /opt/beegfs
DESTDIR ?=

DAEMONS := mgmtd meta storage helperd mon
UTILS := fsck ctl event_listener $(if $(WITHOUT_COMM_DEBUG),,comm_debug)

# exclude components with no runnable tests from `test' and do not provide `ctl-test'.
DO_NOT_TEST := thirdparty event_listener

ALL_COMPONENTS := thirdparty common $(DAEMONS) $(UTILS)
TIDY_COMPONENTS := $(filter-out thirdparty event_listener, $(ALL_COMPONENTS))

all: daemons utils client

.PHONY: daemons
daemons: $(patsubst %,%-all,$(DAEMONS))
	@

.PHONY: utils
utils: $(patsubst %,%-all,$(UTILS))
	@

.PHONY: $(patsubst %,%-all,$(DAEMONS) $(UTILS))
$(patsubst %,%-all,$(DAEMONS) $(UTILS)): common-all
	$(MAKE) -C $(subst -all,,$@)/build all

.PHONY: common-all common
common-all: thirdparty
	$(MAKE) -C common/build all

.PHONY: thirdparty
thirdparty:
	$(MAKE) -C thirdparty/build all $(BEEGFS_THIRDPARTY_OPTIONAL)

.PHONY: client
client:
	$(MAKE) -C client_module/build

.PHONY: tidy
tidy: $(addsuffix -tidy,$(TIDY_COMPONENTS))
	@

define tidy_component
.PHONY: $1-tidy
$1-tidy:
	+$(MAKE) -C $1/build tidy
endef

$(foreach C,$(TIDY_COMPONENTS),$(eval $(call tidy_component,$(C))))

_tested_components := $(filter-out $(DO_NOT_TEST),$(ALL_COMPONENTS))

.PHONY: test
test: $(patsubst %,%-test,$(_tested_components))
	@

define test_component
.PHONY: $1-test
$1-test: $1-all
	cd $1/build && ./test-runner --compiler
endef

$(foreach C,$(_tested_components),$(eval $(call test_component,$(C))))

.PHONY: install
install: daemons-install utils-install common-install client-install event_listener-install
	@

define install_component
.PHONY: $1-install
$1-install: $1-all
	install -t $(DESTDIR)/$(PREFIX)/$2 -D \
		$1/build/beegfs-$$(or $$(install_name),$1)
endef

comm_debug-install: install_name=comm-debug

$(foreach D,$(DAEMONS),$(eval $(call install_component,$D,sbin)))
$(foreach U,$(filter-out event_listener,$(UTILS)),$(eval $(call install_component,$U,sbin)))

.PHONY: daemons-install
daemons-install: $(patsubst %,%-install,$(DAEMONS))
	@

.PHONY: utils-install
utils-install: $(patsubst %,%-install,$(UTILS))
	@

.PHONY: common-install
common-install: common-all
	install -t $(DESTDIR)/$(PREFIX)/lib -D \
		common/build/libbeegfs_ib.so

.PHONY: client-install
client-install: client
	install -t $(DESTDIR)/$(PREFIX)/lib/modules/$(KVER)/kernel/beegfs -D \
		client_module/build/beegfs.ko

## Overriding previous generic rule due to non-matching executable name
.PHONY: event_listener-intsall
event_listener-install: event_listener-all
	install -t $(DESTDIR)/$(PREFIX)/sbin -D \
		event_listener/build/beegfs-event-listener

.PHONY: clean
clean: $(patsubst %,%-clean,$(ALL_COMPONENTS)) client-clean
	@

.PHONY: $(patsubst %,%-clean,$(ALL_COMPONENTS))
$(patsubst %,%-clean,$(ALL_COMPONENTS)):
	$(MAKE) -C $(subst -clean,,$@)/build clean

.PHONY: client-clean
client-clean:
	$(MAKE) -C client_module/build clean

# use DEBUILD_OPTS to pass more options to debuild, eg
#   DEBUILD_OPTS='-j32 --prepend-path=/usr/lib/ccache'
# for greater concurrency during build and ccache support
.PHONY: package-deb
package-deb: clean
	[ '$(PACKAGE_DIR)' ] || { echo need a PACKAGE_DIR >&2; false; }
	! [ -d '$(PACKAGE_DIR)' ] || { echo choose a new directory for PACKAGE_DIR, please >&2; false; }
	mkdir -p '$(PACKAGE_DIR)'
	cd '$(PACKAGE_DIR)' && \
		dpkg-source -I '-I$(PACKAGE_DIR)' -z1 -b '$(dir $(realpath $(firstword $(MAKEFILE_LIST))))' && \
		rm -rf build && \
		dpkg-source -x *.dsc build && \
		( \
			cd build; \
			sed -i -e 's/beegfs (.*)/beegfs ($(BEEGFS_VERSION_DEB))/' debian/changelog; \
			sed -i -e 's/@DATE/$(shell date -R)/' debian/changelog; \
			debuild -eBEEGFS_\* $(DEBUILD_OPTS) -us -uc -b \
		) && \
		rm -rf build *.dsc *.tar.gz

# use RPMBUILD_OPTS to pass more options to rpmuild, eg
#   RPMBUILD_OPTS='-D "MAKE_CONCURRENCY 32"'
# for greater concurrency during build
.PHONY: package-rpm
package-rpm: clean
	[ '$(PACKAGE_DIR)' ] || { echo need a PACKAGE_DIR >&2; false; }
	! [ -d '$(PACKAGE_DIR)' ] || { echo choose a new directory for PACKAGE_DIR, please >&2; false; }
	mkdir -p $(PACKAGE_DIR)/SOURCES
	tar --exclude $(PACKAGE_DIR) --exclude .git --exclude .ccache \
		-cf $(PACKAGE_DIR)/SOURCES/beegfs-$(BEEGFS_VERSION_RPM).tar .
	rpmbuild --clean -bb beegfs.spec \
		--define '_topdir $(abspath $(PACKAGE_DIR))' \
		--define 'EPOCH $(BEEGFS_EPOCH)' \
		--define 'BEEGFS_VERSION $(BEEGFS_VERSION_RPM)' \
		$(RPMBUILD_OPTS)
