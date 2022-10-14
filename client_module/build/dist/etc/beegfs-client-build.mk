# BeeGFS client module DKMS build configuration
# This file is only used when building via DKMS.
# The module needs to be rebuilt after this file has been changed.

# If using thirdparty OFED specify the path to the installation here.
# Examples:
#OFED_INCLUDE_PATH=/usr/src/ofa_kernel/default/include
#OFED_INCLUDE_PATH=/usr/src/openib/include
ifneq ($(OFED_INCLUDE_PATH),)
export KBUILD_EXTRA_SYMBOLS += $(OFED_INCLUDE_PATH)/../Module.symvers
endif
# If building nvidia-fs support, specify path to nvfs.h
#NVFS_H_PATH=/usr/src/mlnx-ofed-kernel-5.4/drivers/nvme/host

