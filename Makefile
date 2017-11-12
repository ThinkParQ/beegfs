export BEEGFS_DEBUG
export USER_CXXFLAGS
export USER_LDFLAGS

export KDIR
export KSRCDIR

PREFIX ?= /opt/beegfs
DESTDIR ?=

DAEMONS := mgmtd meta storage helperd admon mon
UTILS := fsck ctl

# ctl has no runnable tests, exclude it from `test' and do not provide `ctl-test'.
# same for thirdparty and opentk.
DO_NOT_TEST := ctl thirdparty opentk_lib

ALL_COMPONENTS := thirdparty opentk_lib common $(DAEMONS) $(UTILS)

all: daemons utils client

.PHONY: daemons
daemons: $(patsubst %,%-all,$(DAEMONS))
	@

.PHONY: utils
utils: $(patsubst %,%-all,$(UTILS))
	@

.PHONY: $(patsubst %,%-all,$(DAEMONS) $(UTILS))
$(patsubst %,%-all,$(DAEMONS) $(UTILS)): common-all opentk_lib-all
	$(MAKE) -C beegfs_$(subst -all,,$@)/build all

.PHONY: opentk_lib-all
opentk_lib-all:
	$(MAKE) -C beegfs_opentk_lib/build all

.PHONY: common-all common
common-all: thirdparty opentk_lib-all
	$(MAKE) -C beegfs_common/build all

.PHONY: thirdparty
thirdparty:
	$(MAKE) -C beegfs_thirdparty/build all cppunit sqlite

.PHONY: client
client:
	$(MAKE) -C beegfs_client_module/build


_tested_components := $(filter-out $(DO_NOT_TEST),$(ALL_COMPONENTS))

.PHONY: test
test: $(patsubst %,%-test,$(_tested_components))
	@

define test_component
.PHONY: $1-test
$1-test: $1-all
	cd beegfs_$1/build && ./test-runner --compiler
endef

$(foreach C,$(_tested_components),$(eval $(call test_component,$(C))))

.PHONY: install
install: daemons-install utils-install opentk_lib-install client-install
	@

define install_component
.PHONY: $1-install
$1-install: $1-all
	install -t $(DESTDIR)/$(PREFIX)/$2 -D \
		beegfs_$1/build/beegfs-$1
endef

$(foreach D,$(DAEMONS),$(eval $(call install_component,$D,sbin)))
$(foreach U,$(UTILS),$(eval $(call install_component,$U,sbin)))

.PHONY: daemons-install
daemons-install: $(patsubst %,%-install,$(DAEMONS))
	@

.PHONY: utils-install
utils-install: $(patsubst %,%-install,$(UTILS))
	@

.PHONY: opentk_lib-install
opentk_lib-install: opentk_lib-all
	install -t $(DESTDIR)/$(PREFIX)/lib -D \
		beegfs_opentk_lib/build/libbeegfs-opentk.so

.PHONY: client-install
client-install: client
	install -t $(DESTDIR)/$(PREFIX)/lib/modules/$(KVER)/kernel/beegfs -D \
		beegfs_client_module/build/beegfs.ko


.PHONY: clean
clean: $(patsubst %,%-clean,$(ALL_COMPONENTS)) client-clean
	@

.PHONY: $(patsubst %,%-clean,$(ALL_COMPONENTS))
$(patsubst %,%-clean,$(ALL_COMPONENTS)):
	$(MAKE) -C beegfs_$(subst -clean,,$@)/build clean

.PHONY: client-clean
client-clean:
	$(MAKE) -C beegfs_client_module/build clean
