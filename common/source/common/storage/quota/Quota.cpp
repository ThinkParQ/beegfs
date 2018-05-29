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
      return stream << "ZFS";
   case QuotaBlockDeviceFsType_ZFSOLD:
      return stream << "ZFS<0.7.4";
   }

   return stream << "Invalid";
}
