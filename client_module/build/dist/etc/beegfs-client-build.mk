# BeeGFS client module DKMS build configuration
# This file is only used when building via DKMS.
# The module needs to be rebuilt after this file has been changed.

# If using thirdparty OFED specify the path to the installation here.
# Examples:
#OFED_INCLUDE_PATH=/usr/src/ofa_kernel/default/include
#OFED_INCLUDE_PATH=/usr/src/openib/include
# To disable RDMA support, define BEEGFS_NO_RDMA
#BEEGFS_NO_RDMA=1
# If building nvidia-fs support, specify path to nvfs-dma.h.
# This directory must also have config-host.h, which is created
# by the nvidia-fs configure script.
# Example:
#NVFS_INCLUDE_PATH=/usr/src/nvidia-fs-2.13.5
# If building nvidia-fs support, specify path to NVIDIA driver
# source.
# Example:
#NVIDIA_INCLUDE_PATH=/usr/src/nvidia-520.61.05/nvidia
