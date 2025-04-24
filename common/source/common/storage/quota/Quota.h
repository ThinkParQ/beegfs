#pragma once

#include <ostream>

enum QuotaBlockDeviceFsType
{
   QuotaBlockDeviceFsType_UNKNOWN=0,
   QuotaBlockDeviceFsType_EXTX=1,
   QuotaBlockDeviceFsType_XFS=2,
   QuotaBlockDeviceFsType_ZFS=3,
   QuotaBlockDeviceFsType_ZFSOLD=4 // zfs < 0.7.4 (no inode quota)
};

std::ostream& operator<<(std::ostream& stream, const QuotaBlockDeviceFsType type);

enum QuotaInodeSupport
{
   QuotaInodeSupport_UNKNOWN=0,
   QuotaInodeSupport_ALL_BLOCKDEVICES=1,
   QuotaInodeSupport_SOME_BLOCKDEVICES=2,
   QuotaInodeSupport_NO_BLOCKDEVICES=3
};

