#ifndef FHGFSOPS_IOCTL_H
#define FHGFSOPS_IOCTL_H

#include <asm/ioctl.h>
#include <linux/kernel.h>

#include <common/nodes/NumNodeID.h>
#include <uapi/beegfs_client.h>

#ifdef FS_IOC_GETVERSION
   #define BEEGFS_IOC_GETVERSION_OLD          FS_IOC_GETVERSION // predefined in linux/fs.h
#else // old kernels didn't have FS_IOC_GETVERSION
   #define BEEGFS_IOC_GETVERSION_OLD          _IOR('v', BEEGFS_IOCNUM_GETVERSION_OLD, long)
#endif

#ifdef CONFIG_COMPAT
   #ifdef FS_IOC32_GETVERSION
      #define BEEGFS_IOC32_GETVERSION_OLD     FS_IOC32_GETVERSION // predefined in linux/fs.h
   #else // old kernels didn't have FS_IOC32_GETVERSION
      #define BEEGFS_IOC32_GETVERSION_OLD     _IOR('v', BEEGFS_IOCNUM_GETVERSION_OLD, int)
   #endif
#endif


extern long FhgfsOpsIoctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
   extern long FhgfsOpsIoctl_compatIoctl(struct file *file, unsigned int cmd, unsigned long arg);
#endif


#endif // FHGFSOPS_IOCTL_H
