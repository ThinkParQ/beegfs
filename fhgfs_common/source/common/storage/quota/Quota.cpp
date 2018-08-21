#include "Quota.h"

std::ostream& operator<<(std::ostream& stream, const QuotaBlockDeviceFsType type)
{

   switch (type)
   {
      case QuotaBlockDeviceFsType_UNKNOWN:
         return stream << "Unknown";
      case QuotaBlockDeviceFsType_EXTX:
         return stream << "Ext2/3/4";
      case QuotaBlockDeviceFsType_XFS:
         return stream << "XFS";
      case QuotaBlockDeviceFsType_ZFS:
      case QuotaBlockDeviceFsType_ZFSOLD: // fallthrough
         return stream << "ZFS";
   }

   return stream << "Invalid";
}
