
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
export BEEGFS_NO_RDMA

BEEGFS_DKMS_BUILD=0
ifdef KERNELRELEASE
BEEGFS_DKMS_BUILD=1
endif

ifeq ($(BEEGFS_DKMS_BUILD),1)
-include /etc/beegfs/beegfs-client-build.mk
endif
-include Version.mk

ifeq ($(obj),)
BEEGFS_BUILDDIR := $(shell pwd)
else
BEEGFS_BUILDDIR := $(obj)
endif

ifeq ($(KRELEASE),)
KRELEASE := $(shell uname -r)
endif

ifneq ($(BEEGFS_NO_RDMA),)
BEEGFS_CFLAGS += -DBEEGFS_NO_RDMA
else

$(info $$OFED_INCLUDE_PATH = [${OFED_INCLUDE_PATH}])

ifneq ($(OFED_INCLUDE_PATH),)
BEEGFS_CFLAGS += -I$(OFED_INCLUDE_PATH)
export KBUILD_EXTRA_SYMBOLS += $(OFED_INCLUDE_PATH)/../Module.symvers
endif
endif

# The following section deals with the auto-detection of the kernel
# build directory (KDIR)

# Guess KDIR based on running kernel.
# - "/usr/src/linux-headers-*" for Ubuntu
# - "/usr/src/kernels/*" for RHEL
# - "/lib/modules/*/build" for Debian, SLES
ifeq ($(KDIR),)
override KDIR = \
	/lib/modules/$(KRELEASE)/build \
	/lib/modules/default/build \
	/usr/src/linux-headers-$(KRELEASE) \
	/usr/src/linux-headers-default \
	/usr/src/kernels/$(KRELEASE) \
	/usr/src/kernels/default
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

ifeq ($(BEEGFS_NO_RDMA),)
# OFED
ifneq ($(OFED_INCLUDE_PATH),)

BEEGFS_CFLAGS += -I$(OFED_INCLUDE_PATH)

module: $(OFED_INCLUDE_PATH)/rdma/rdma_cm.h
$(OFED_INCLUDE_PATH)/rdma/rdma_cm.h:
	$(error OFED_INCLUDE_PATH not valid: $(OFED_INCLUDE_PATH))
endif
endif

# Include kernel feature auto-detectors
include KernelFeatureDetection.mk

KMOD_INST_DIR ?= $(DESTDIR)/lib/modules/$(KRELEASE)/updates/fs/beegfs

# Prepare CFLAGS:
# (Note: "-Wsign-compare" included in "-Wextra", but must be explicit here,
# because kernel Makefile adds "-Wno-sign-compare" by default. But we can't
# make it permanent here, because it generates a lot of warnings from kernel
# includes.)
BEEGFS_CFLAGS  :=  $(BUILD_ARCH) $(KERNEL_FEATURE_DETECTION) \
	-I$(BEEGFS_BUILDDIR)/../source \
	-I$(BEEGFS_BUILDDIR)/../include \
	-Wextra -Wno-sign-compare -Wno-empty-body -Wno-unused-parameter -Wno-missing-field-initializers \
	-DBEEGFS_MODULE_NAME_STR='\"$(TARGET)\"'

# Update 2022-12: BeeGFS module source code had already switched to -std=gnu99,
# but now the kernel has caught up with us - kernel moved from gnu89 to
# gnu11.
# So we're switching from gnu99 to gnu11, to not break the build with the newer
# kernels. We still need to specify this flag because that would break the
# client module build with older kernels, where gnu89 is the default.
BEEGFS_CFLAGS += -std=gnu11

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

ifeq ($(BEEGFS_NO_RDMA),)
# NVFS
ifneq ($(NVFS_INCLUDE_PATH),)
$(NVFS_INCLUDE_PATH)/nvfs-dma.h:
	$(error NVFS_INCLUDE_PATH missing nvfs-dma.h: $(NVFS_INCLUDE_PATH))
$(NVFS_INCLUDE_PATH)/config-host.h:
	$(error NVFS_INCLUDE_PATH missing config-host.h: $(NVFS_INCLUDE_PATH))
$(NVIDIA_INCLUDE_PATH)/nv-p2p.h:
	$(error NVIDIA_INCLUDE_PATH missing nv-p2p.h: $(NVIDIA_INCLUDE_PATH))
module: $(NVFS_INCLUDE_PATH)/nvfs-dma.h  $(NVFS_INCLUDE_PATH)/config-host.h \
	$(NVIDIA_INCLUDE_PATH)/nv-p2p.h

BEEGFS_CFLAGS += -DBEEGFS_NVFS
BEEGFS_CFLAGS += -I$(NVFS_INCLUDE_PATH)  -I$(NVIDIA_INCLUDE_PATH)
endif

endif

# if path to strip command was not given, use default
# (alternative strip is important when cross-compiling)
ifeq ($(STRIP),)
STRIP=strip
endif

BEEGFS_CFLAGS += '-DBEEGFS_VERSION=\"$(BEEGFS_VERSION)\"'

# Prepare RELEASE_PATH extension
ifneq ($(RELEASE_PATH),)
RELEASE_PATH_CLIENT := $(RELEASE_PATH)/client_module_$(shell echo '$(BEEGFS_VERSION)' | cut -d. -f1)
endif


all: module
	@ /bin/true

module: $(TARGET_ALL_DEPS)
ifeq ($(KDIR_PRUNED_HEAD),)
	$(error Linux kernel build directory not found. Please check if\
	the kernel module development packages are installed for the current kernel\
	version. (RHEL: kernel-devel; SLES: kernel-default-devel; Debian: linux-headers))
endif

ifeq ($(KSRCDIR_PRUNED_HEAD),)
	$(error Linux kernel source directory not found. Please check if\
	the kernel module development packages are installed for the current kernel\
	version. (RHEL: kernel-devel; SLES: kernel-default-devel; Debian: linux-headers))
endif

	@echo "Building beegfs client module"
	$(MAKE) -C $(KDIR_PRUNED_HEAD) "M=$(BEEGFS_BUILDDIR)/../source" \
	"EXTRA_CFLAGS=$(BEEGFS_CFLAGS) $(EXTRA_CFLAGS)"  modules

	@cp ../source/$(TARGET).ko .
	@ cp ${TARGET}.ko ${TARGET}-unstripped.ko
	@ ${STRIP} --strip-debug ${TARGET}.ko;

coccicheck:
	$(MAKE) -C $(KDIR_PRUNED_HEAD) "M=$(BEEGFS_BUILDDIR)"  coccicheck MODE=report \
		M=$(BEEGFS_BUILDDIR)/../source KBUILD_EXTMOD="$(BEEGFS_BUILDDIR)/../source"


include AutoRebuild.mk # adds auto_rebuild targets

prepare_release:
ifeq ($(RELEASE_PATH),)
	$(error RELEASE_PATH not defined)
endif

	@ echo "Creating release directory:" $(RELEASE_PATH_CLIENT)
	mkdir --parents $(RELEASE_PATH_CLIENT)/build \
		$(RELEASE_PATH_CLIENT)/source \
		$(RELEASE_PATH_CLIENT)/include

	@ echo "Storing beegfs version:" $(BEEGFS_VERSION)
	echo "BEEGFS_VERSION =" $(BEEGFS_VERSION) > $(RELEASE_PATH_CLIENT)/build/Version.mk

	@ echo "Copying beegfs client release files to" $(RELEASE_PATH_CLIENT) "..."
	cp Makefile $(RELEASE_PATH_CLIENT)/build/Makefile
	cp KernelFeatureDetection.mk $(RELEASE_PATH_CLIENT)/build/
	cp AutoRebuild.mk $(RELEASE_PATH_CLIENT)/build/
	cp feature-detect.sh $(RELEASE_PATH_CLIENT)/build/
	cp ../source/Makefile $(RELEASE_PATH_CLIENT)/source/

	find ../source -mount -name '*.h' -type f | \
		xargs -I ’{}’ cp --parents ’{}’ $(RELEASE_PATH_CLIENT)/build
	find ../source -mount -name '*.c' -type f | \
		xargs -I ’{}’ cp --parents ’{}’ $(RELEASE_PATH_CLIENT)/build
	find ../include -mount -name '*.h' -type f | \
		xargs -I ’{}’ cp --parents ’{}’ $(RELEASE_PATH_CLIENT)/build

install:
	install -D -m 644 $(TARGET).ko $(KMOD_INST_DIR)/$(TARGET).ko
	depmod -a $(KRELEASE)

clean:
	rm -f *~ .${TARGET}??*
	rm -f .*.cmd *.mod.c *.mod.o *.o *.ko *.ko.unsigned
	rm -f ../source/Module*.symvers ../source/modules.order ../source/Module.markers
	rm -f Module*.symvers modules.order Module.markers
	rm -f $(AUTO_REBUILD_KVER_FILE)
	rm -rf .tmp_versions/
	find ../source/ -mount -name '*.o' -type f -delete
	find ../source/ -mount -name '.*.o.cmd' -type f -delete
	find ../source/ -mount -name '.*.o.d' -type f -delete
	find ../source/ -mount -name '*.gcno' -type f -delete

help:
	@echo "This makefile creates the kernel module: $(TARGET) (beegfs-client)"
	@echo ' '
	@echo 'client Arguments (required):'
	@echo '  RELEASE_PATH=<path> (Target: prepare_release)'
	@echo '    The path to the client release directory.'
	@echo ' '
	@echo 'client Arguments (optional):'
	@echo '   KRELEASE=<release>: Kernel release'
	@echo '     (The output of "uname -r" will be used if undefined.'
	@echo '      This option is useful when building for a kernel different'
	@echo '      from the one currently running (e.g. in a chroot).)'
	@echo '   KDIR=<path>: Kernel build directory.'
	@echo '     (Will be guessed based on running kernel or KRELEASE if undefined.)'
	@echo '   KSRCDIR=<path>: Kernel source directory containing the kernel include '
	@echo '     directory. (Will be guessed based on KDIR if undefined.)'
	@echo '   BEEGFS_DEBUG=1:'
	@echo '     Enables file sytem debug log messages etc.'
	@echo '   TARGET=<MODULE_NAME>'
	@echo '     Set a different module and file system name.'
	@echo ' '
	@echo 'Infiniband (RDMA) arguments (optional):'
	@echo '   OFED_INCLUDE_PATH=<path>:'
	@echo '     Path to OpenFabrics Enterpise Distribution kernel include directory, e.g.'
	@echo '     "/usr/src/openib/include". (If not defined, the standard kernel headers'
	@echo '      will be used.)'
	@echo ''
	@echo 'NVIDIA GPUDirect Storage (GDS) arguments (optional):'
	@echo '  NVFS_INCLUDE_PATH=<path>:'
	@echo '    Path to directory that contains nvfs-dma.h. If not defined, GDS support is'
	@echo '    disabled.'
	@echo '  NVIDIA_INCLUDE_PATH=<path>:'
	@echo '    Path to NVIDIA driver source. Required when NVFS_INCLUDE_PATH is specifed.'
	@echo ''
	@echo 'Targets:'
	@echo '   all (default)     - build only'
	@echo '   install           - install the kernel modules'
	@echo '   clean             - delete previously compiled files'
	@echo '   prepare_release   - build and copy files into the RELEASE_PATH directory'
