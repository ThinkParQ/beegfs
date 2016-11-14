#ifndef COMMON_STORAGE_QUOTA_QUOTA_H_
#define COMMON_STORAGE_QUOTA_QUOTA_H_

enum QuotaBlockDeviceFsType
{
   QuotaBlockDeviceFsType_UNKNOWN=0,
   QuotaBlockDeviceFsType_EXTX=1,
   QuotaBlockDeviceFsType_XFS=2,
   QuotaBlockDeviceFsType_ZFS=3
};

enum QuotaInodeSupport
{
   QuotaInodeSupport_UNKNOWN=0,
   QuotaInodeSupport_ALL_BLOCKDEVICES=1,
   QuotaInodeSupport_SOME_BLOCKDEVICES=2,
   QuotaInodeSupport_NO_BLOCKDEVICES=3
};

#endif /* COMMON_STORAGE_QUOTA_QUOTA_H_ */
