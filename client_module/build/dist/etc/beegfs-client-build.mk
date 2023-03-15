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
# To disable RDMA support, define BEEGFS_NO_RDMA
#BEEGFS_NO_RDMA=1
