# This is the BeeGFS client makefile.
# It creates the client kernel module (beegfs-$(VERSION).ko).
#
# Use "make help" to find out about configuration options.

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

KMOD_INST_DIR=$(DESTDIR)/lib/modules/$(shell uname -r)/updates/fs/beegfs_autobuild

# Include kernel feature auto-detectors
include KernelFeatureDetection.mk

# Include version file for definition of BEEGFS_VERSION
include Version.mk

# Prepare CFLAGS:
BEEGFS_CFLAGS  := $(BUILD_ARCH) $(KERNEL_FEATURE_DETECTION) \
	-I$(BEEGFS_BUILDDIR)/../source \
	-Wno-unused-parameter -DBEEGFS_MODULE_NAME_STR='\"$(TARGET)\"'
BEEGFS_CFLAGS_DEBUG := -g3 -rdynamic -fno-inline -DBEEGFS_DEBUG \
	-DLOG_DEBUG_MESSAGES -DDEBUG_REFCOUNT -DBEEGFS_OPENTK_LOG_CONN_ERRORS
BEEGFS_CFLAGS_RELEASE := -Wuninitialized

ifeq ($(BEEGFS_DEBUG),)
BEEGFS_CFLAGS += $(BEEGFS_CFLAGS_RELEASE)
else
BEEGFS_CFLAGS += $(BEEGFS_CFLAGS_DEBUG)
endif

ifneq ($(BEEGFS_VERSION),)
BEEGFS_CFLAGS += '-DBEEGFS_VERSION=\"$(BEEGFS_VERSION)\"'
endif

# OFED
ifneq ($(OFED_INCLUDE_PATH),)
BEEGFS_CFLAGS += -I$(OFED_INCLUDE_PATH)

module: $(OFED_INCLUDE_PATH)/rdma/rdma_cm.h
$(OFED_INCLUDE_PATH)/rdma/rdma_cm.h:
	$(error OFED_INCLUDE_PATH not valid: $(OFED_INCLUDE_PATH))
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

all: module
	@ /bin/true


module: $(TARGET_ALL_DEPS)
ifeq ($(KDIR_PRUNED_HEAD),)
	$(error Linux kernel build directory not found. Please check if\
	the kernel module development packages are installed for the current kernel\
	version. (RHEL: kernel-devel; SLES: linux-kernel-headers, kernel-source;\
	Debian: linux-headers))
endif

ifeq ($(KSRCDIR_PRUNED_HEAD),)
	$(error Linux kernel source directory not found. Please check if\
	the kernel module development packages are installed for the current kernel\
	version. (RHEL: kernel-devel; SLES: linux-kernel-headers, kernel-source;\
	Debian: linux-headers))
endif

ifneq ($(OFED_INCLUDE_PATH),)
	if [ -f $(OFED_INCLUDE_PATH)/../Module.symvers ]; then \
		cp $(OFED_INCLUDE_PATH)/../Module.symvers ../source ; \
	fi
endif

	@echo "Building beegfs client module"
	$(MAKE) -C $(KDIR_PRUNED_HEAD) SUBDIRS=$(BEEGFS_BUILDDIR)/../source \
	"EXTRA_CFLAGS=$(BEEGFS_CFLAGS)" modules

	@cp ../source/$(TARGET).ko .
	@ cp ${TARGET}.ko ${TARGET}-unstripped.ko
	@ ${STRIP} --strip-debug ${TARGET}.ko


include AutoRebuild.mk # adds auto_rebuild targets


install:
	install -D -m 644 $(TARGET).ko $(KMOD_INST_DIR)/$(TARGET).ko
	depmod -a



clean:
	rm -f *~ .${TARGET}??*
	rm -f .*.cmd *.mod.c *.mod.o *.o *.ko *.ko.unsigned
	rm -f Module*.symvers modules.order Module.markers
	rm -f $(AUTO_REBUILD_KVER_FILE)
	rm -rf .tmp_versions/
	find ../source/ -mount -name '*.o' -type f -delete
	find ../source/ -mount -name '.*.o.cmd' -type f -delete
	find ../source/ -mount -name '.*.o.d' -type f -delete

	rm -f $(BEEGFS_AUTOCONF_BUILD_REL_PATH)


help:
	@echo "This makefile creates the kernel module: $(TARGET) (beegfs-client)"
	@echo ' '
	@echo 'client Arguments (optional):'
	@echo '  KDIR=<path>: Kernel build directory.'
	@echo '    (Will be guessed based on running kernel if undefined.)'
	@echo '  KSRCDIR=<path>: Kernel source directory containing the kernel include files.'
	@echo '    (Will be guessed based on KDIR if undefined.)'
	@echo '   TARGET=<MODULE_NAME>'
	@echo '     Set a different module and file system name.'
	@echo ' '
	@echo 'Infiniband (RDMA) arguments (optional):'
	@echo '  OFED_INCLUDE_PATH=<path>:'
	@echo '    Path to OpenFabrics Enterpise Distribution kernel include directory, e.g.'
	@echo '    "/usr/src/openib/include". (If not defined, the standard kernel headers'
	@echo '     will be used.)'
	@echo '  BEEGFS_OFED_1_2_API={1,2}:'
	@echo '    Defining this enables OFED 1.2.0 ibverbs API compatibility.'
	@echo '    (If not defined, OFED 1.2.5 or higher API will be used.)'
