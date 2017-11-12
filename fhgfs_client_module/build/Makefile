# This is the BeeGFS client makefile.
# It creates the client kernel module (beegfs.ko).
#
# Use "make help" to find out about configuration options.
#
# Note: This is the Makefile for internal use, there is a separate Release.mk
# file for release packages (to handle the closed source tree properly).

TARGET ?= beegfs

export TARGET
export OFED_INCLUDE_PATH

ifeq ($(obj),)
BEEGFS_BUILDDIR := $(shell pwd)
else
BEEGFS_BUILDDIR := $(obj)
endif


# The following section deals with the auto-detection of the kernel
# build directory (KDIR)

# Guess KDIR based on running kernel
# (This is usually /lib/modules/<kernelversion>/build, but you can specify
# multiple directories here as a space-separated list)
ifeq ($(KDIR),)
KDIR = /lib/modules/$(shell uname -r)/build /usr/src/linux-headers-$(shell uname -r) \
       /usr/src/kernels/$(shell uname -r)
endif

# Prune the KDIR list down to paths that exist and have an
# /include/linux/version.h file
# Note: linux-3.7 moved version.h to generated/uapi/linux/version.h
test_dir = $(shell [ -e $(dir)/include/linux/version.h -o \
	-e $(dir)/include/generated/uapi/linux/version.h ] && echo $(dir) )
KDIR_PRUNED := $(foreach dir, $(KDIR), $(test_dir) )

# We use the first valid entry of the pruned KDIR list
KDIR_PRUNED_HEAD := $(firstword $(KDIR_PRUNED) )


# The following section deals with the auto-detection of the kernel
# source path (KSRCDIR) which is required e.g. for KERNEL_FEATURE_DETECTION.

# Guess KSRCDIR based on KDIR
# (This is usually KDIR or KDIR/../source, so you can specify multiple
# directories here as a space-separated list)
ifeq ($(KSRCDIR),)

# Note: "KSRCDIR += $(KDIR)/../source" is not working here
# because of the symlink ".../build"), so we do it with substring
# replacement

KSRCDIR := $(subst /build,/source, $(KDIR_PRUNED_HEAD) )
KSRCDIR += $(KDIR)
endif

# Prune the KSRCDIR list down to paths that exist and contain an
# include/linux/fs.h file
test_dir = $(shell [ -e $(dir)/include/linux/fs.h ] && echo $(dir) )
KSRCDIR_PRUNED := $(foreach dir, $(KSRCDIR), $(test_dir) )

# We use the first valid entry of the pruned KSRCDIR list
KSRCDIR_PRUNED_HEAD := $(firstword $(KSRCDIR_PRUNED) )


# Include kernel feature auto-detectors
include KernelFeatureDetection.mk

BEEGFS_VERSION_PATH       ?= $(BEEGFS_BUILDDIR)/../../beegfs-version

KMOD_INST_DIR=$(DESTDIR)/lib/modules/$(shell uname -r)/updates/fs/beegfs

# Prepare CFLAGS:
# (Note: "-Wsign-compare" included in "-Wextra", but must be explicit here,
# because kernel Makefile adds "-Wno-sign-compare" by default. But we can't
# make it permanent here, because it generates a lot of warnings from kernel
# includes.)
BEEGFS_CFLAGS  :=  $(BUILD_ARCH) $(KERNEL_FEATURE_DETECTION) \
	-I$(BEEGFS_BUILDDIR)/../source \
	-Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
	-DBEEGFS_MODULE_NAME_STR='\"$(TARGET)\"'

ifeq ($(shell echo | gcc -Wtype-limits -E - >/dev/null 2>&1 && echo 1),1)
   BEEGFS_CFLAGS += -Wno-type-limits
endif

# -O0 would be better, but is not allowed by kernel includes (will not work)
BEEGFS_CFLAGS_DEBUG := -O1 -ggdb3 -rdynamic -fno-inline -DBEEGFS_DEBUG \
	-DLOG_DEBUG_MESSAGES -DDEBUG_REFCOUNT -DBEEGFS_LOG_CONN_ERRORS
BEEGFS_CFLAGS_RELEASE := -Wuninitialized

ifeq ($(BEEGFS_DEBUG),)
BEEGFS_CFLAGS += $(BEEGFS_CFLAGS_RELEASE)
else
BEEGFS_CFLAGS += $(BEEGFS_CFLAGS_DEBUG)
endif

# OFED
ifeq ($(BEEGFS_OPENTK_IBVERBS),1)
BEEGFS_CFLAGS += -DBEEGFS_OPENTK_IBVERBS
endif

# OFED API version
ifneq ($(BEEGFS_OFED_1_2_API),)
BEEGFS_CFLAGS += "-DBEEGFS_OFED_1_2_API=$(BEEGFS_OFED_1_2_API)"
endif


# if path to strip command was not given, use default
# (alternative strip is important when cross-compiling)
ifeq ($(STRIP),)
STRIP=strip
endif

# beegfs version
BEEGFS_VERSION := $(shell bash -c "BEEGFS_VERSION=$(BEEGFS_VERSION) $(BEEGFS_VERSION_PATH) --print_version || echo **FAIL**")
BEEGFS_VERSION_CODE := $(shell bash -c "BEEGFS_VERSION=$(BEEGFS_VERSION) $(BEEGFS_VERSION_PATH) --print_version_code || echo **FAIL**")
BEEGFS_MAJOR_VERSION := $(shell bash -c "BEEGFS_VERSION=$(BEEGFS_VERSION) $(BEEGFS_VERSION_PATH) --print_major_version || echo **FAIL**")

BEEGFS_CFLAGS += '-DBEEGFS_VERSION=\"$(BEEGFS_VERSION)\"' -DBEEGFS_VERSION_CODE=$(BEEGFS_VERSION_CODE)

# Prepare RELEASE_PATH extension
ifneq ($(RELEASE_PATH),)
RELEASE_PATH_CLIENT := $(RELEASE_PATH)/beegfs_client_module_$(BEEGFS_MAJOR_VERSION)
endif


all: test_version module
	@ /bin/true

test_version:
	@# just call the beegfs-version script to check whether it returns an error. the script output is
	@# used in other places of this makefile and we want to make sure that we don't mess up version
	@# tags in the binary with script errors.
	@echo "beegfs version check..."
	@$(BEEGFS_VERSION_PATH) --verbose

module: $(TARGET_ALL_DEPS)
	@if [ -z "$(KDIR_PRUNED_HEAD)" ]; then \
		echo "*** ERROR: Linux kernel build directory not found. Please check if" \
		"the kernel module development packages are installed for the current kernel" \
		"version. (RHEL: kernel-devel; SLES: kernel-source; Debian: linux-headers)"; \
		false; \
	fi >&2

	@if [ -z "$(KSRCDIR_PRUNED_HEAD)" ]; then \
		echo "*** ERROR: Linux kernel source directory not found. Please check if" \
		"the kernel module development packages are installed for the current kernel" \
		"version. (RHEL: kernel-devel; SLES: kernel-source; Debian: linux-headers)"; \
		false; \
	fi >&2

ifneq ($(OFED_INCLUDE_PATH),)
	if [ -f $(OFED_INCLUDE_PATH)/../Module.symvers ]; then \
		cp $(OFED_INCLUDE_PATH)/../Module.symvers ../source ; \
	fi
endif

	$(MAKE) -C $(KDIR_PRUNED_HEAD) "SUBDIRS=$(BEEGFS_BUILDDIR)/../source" \
	"EXTRA_CFLAGS=$(BEEGFS_CFLAGS) $(EXTRA_CFLAGS)"  modules

	@cp ../source/$(TARGET).ko .
	@ cp ${TARGET}.ko ${TARGET}-unstripped.ko
	@ ${STRIP} --strip-debug ${TARGET}.ko;

coccicheck:
	$(MAKE) -C $(KDIR_PRUNED_HEAD) "SUBDIRS=$(BEEGFS_BUILDDIR)"  coccicheck MODE=report \
		M=$(BEEGFS_BUILDDIR)/../source KBUILD_EXTMOD="$(BEEGFS_BUILDDIR)/../source"


include AutoRebuild.mk # adds auto_rebuild targets


prepare_release: test_version
ifeq ($(RELEASE_PATH),)
	$(error RELEASE_PATH not defined)
endif

ifeq ($(BEEGFS_VERSION),)
	$(error BEEGFS_VERSION not defined)
endif

ifeq ($(BEEGFS_VERSION_CODE),)
	$(error BEEGFS_VERSION_CODE not defined)
endif

	@ echo "Creating release directory:" $(RELEASE_PATH_CLIENT)
	mkdir --parents $(RELEASE_PATH_CLIENT)/build $(RELEASE_PATH_CLIENT)/source

	@ echo "Storing beegfs version:" $(BEEGFS_VERSION)
	echo "BEEGFS_VERSION =" $(BEEGFS_VERSION) > $(RELEASE_PATH_CLIENT)/build/Version.mk
	echo "BEEGFS_VERSION_CODE =" $(BEEGFS_VERSION_CODE) >> $(RELEASE_PATH_CLIENT)/build/Version.mk

	@ echo "Copying beegfs client release files to" $(RELEASE_PATH_CLIENT) "..."
	cp Release.mk $(RELEASE_PATH_CLIENT)/build/Makefile
	cp KernelFeatureDetection.mk $(RELEASE_PATH_CLIENT)/build/
	cp AutoRebuild.mk $(RELEASE_PATH_CLIENT)/build/
	cp feature-detect.sh $(RELEASE_PATH_CLIENT)/build/
	cp ../source/Makefile $(RELEASE_PATH_CLIENT)/source/

	find ../source -mount -name '*.h' -type f | \
		xargs -I ’{}’ cp --parents ’{}’ $(RELEASE_PATH_CLIENT)/build
	find ../source -mount -name '*.c' -type f | \
		xargs -I ’{}’ cp --parents ’{}’ $(RELEASE_PATH_CLIENT)/build

install:
	install -D -m 644 $(TARGET).ko $(KMOD_INST_DIR)/$(TARGET).ko
	depmod -a

clean:
	rm -f *~ .${TARGET}??*
	rm -f .*.cmd *.mod.c *.mod.o *.o *.ko *.ko.unsigned
	rm -f ../source/Module*.symvers ../source/modules.order ../source/Module.markers
	rm -f $(AUTO_REBUILD_KVER_FILE)
	rm -rf .tmp_versions/
	find ../source/ -mount -name '*.o' -type f -delete
	find ../source/ -mount -name '.*.o.cmd' -type f -delete
	find ../source/ -mount -name '.*.o.d' -type f -delete
	find ../source/ -mount -name '*.gcno' -type f -delete

rpm:
	./make-rpm

deb:
	./make-deb

help:
	@echo "This makefile creates the kernel module: $(TARGET) (beegfs-client)"
	@echo ' '
	@echo 'beegfs_client Arguments (required):'
	@echo '  RELEASE_PATH=<path> (Target: prepare_release)'
	@echo '    The path to the beegfs_client release directory.'
	@echo ' '
	@echo 'beegfs_client Arguments (optional):'
	@echo '   KDIR=<path>: Kernel build directory.'
	@echo '     (Will be guessed based on running kernel if undefined.)'
	@echo '   KSRCDIR=<path>: Kernel source directory containing the kernel include '
	@echo '     directory. (Will be guessed based on KDIR if undefined.)'
	@echo '   BEEGFS_DEBUG=1:'
	@echo '     Enables file sytem debug log messages etc.'
	@echo '   TARGET=<MODULE_NAME>'
	@echo '     Set a different module and file system name.'
	@echo ' '
	@echo 'Infiniband (RDMA) arguments (optional):'
	@echo '   BEEGFS_OPENTK_IBVERBS=1:'
	@echo '     Defining this enables ibverbs support.'
	@echo '   OFED_INCLUDE_PATH=<path>:'
	@echo '     Path to OpenFabrics Enterpise Distribution kernel include directory, e.g.'
	@echo '     "/usr/src/openib/include". (If not defined, the standard kernel headers'
	@echo '      will be used.)'
	@echo '   BEEGFS_OFED_1_2_API={1,2}:'
	@echo '     Defining one of these legacy modes enables OFED 1.2.0 ibverbs API'
	@echo '     compatibility. (If not defined, OFED 1.2.5 or higher API will be used.)'
	@echo ''
	@echo 'Targets:'
	@echo '   all (default)     - build only'
	@echo '   install           - install the kernel modules'
	@echo '   clean             - delete previously compiled files'
	@echo '   prepare_release   - build and copy files into the RELEASE_PATH directory'
	@echo '   rpm               - create a rpm package file'
	@echo '   deb               - build debian package file'
