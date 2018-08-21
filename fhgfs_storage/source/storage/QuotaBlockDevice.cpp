#include <app/App.h>
#include <common/app/log/LogContext.h>
#include <program/Program.h>
#include "QuotaBlockDevice.h"

#include <sys/stat.h>
#include <mntent.h>
#include <fstab.h>

#include <sys/statfs.h>

/**
 * checks one storage target path and creates a QuotaBlockDevice
 *
 * @param targetPath path to block device to check
 * @param targetNumID targetNumID of the storage target to check
 */
QuotaBlockDevice QuotaBlockDevice::getBlockDeviceOfTarget(std::string& targetPath,
   uint16_t targetNumID)
{
   static std::string mountInformationPath("/proc/mounts");

   std::string resMountPath;
   std::string resBlockDevicePath;

   struct mntent* mntData;
   FILE *mntFile = setmntent(mountInformationPath.c_str(), "r");

   while ( (mntData = getmntent(mntFile) ) )
   {
      std::string tmpMountPath(mntData->mnt_dir);

      // test all mounts, use the mount with the longest match and use the last match if multiple
      // mounts with the longest match exists
      if (targetPath != tmpMountPath &&
            tmpMountPath != "/" &&
            targetPath.substr(0, tmpMountPath.size() + 1) != tmpMountPath + '/')
         continue;

      if (resMountPath.size() <= tmpMountPath.size())
      {
         resMountPath = mntData->mnt_dir;
         resBlockDevicePath = mntData->mnt_fsname;
      }
   }

   endmntent(mntFile);

   QuotaBlockDevice blockDevice(resMountPath, resBlockDevicePath, getFsType(targetPath), targetPath);

   // check if the installed libzfs is compatible with implementation
   if(blockDevice.getFsType() == QuotaBlockDeviceFsType_ZFS)
   {
      // note: if the installed zfslib doesn't support inode quota,
      // QuotaBlockDeviceFsType_ZFSOLD will be set inside of checkRequiredLibZfsFunctions
      if(!Program::getApp()->isDlOpenHandleLibZfsValid() )
         QuotaTk::checkRequiredLibZfsFunctions(&blockDevice, targetNumID);
   }

   return blockDevice;
}

/**
 * checks all storage target paths and creates a QuotaBlockDevice
 *
 * @param targetPaths list with the paths of the storage target to check for enabled quota
 * @param outBlockDevices the list with all QuotaBlockDevice
 */
void QuotaBlockDevice::getBlockDevicesOfTargets(TargetPathMap* targetPaths,
   QuotaBlockDeviceMap* outBlockDevices)
{
   for(TargetPathMapIter iter = targetPaths->begin(); iter != targetPaths->end(); iter++)
   {
      uint16_t targetNumID =  iter->first;
      std::string storageTargetPath = iter->second;

      QuotaBlockDevice blockDevice = getBlockDeviceOfTarget(storageTargetPath, targetNumID);
      outBlockDevices->insert(QuotaBlockDeviceMapVal(targetNumID, blockDevice) );
   }
}

QuotaInodeSupport QuotaBlockDevice::updateQuotaInodeSupport(
   QuotaInodeSupport currentQuotaInodeSupport, QuotaBlockDeviceFsType fsTypeBlockDevice)
{
   switch(fsTypeBlockDevice)
   {
      case QuotaBlockDeviceFsType_EXTX: //no break because both FS requires the same handling
      case QuotaBlockDeviceFsType_XFS:
         if(currentQuotaInodeSupport == QuotaInodeSupport_UNKNOWN)
         {
            return QuotaInodeSupport_ALL_BLOCKDEVICES;
         }
         else if(currentQuotaInodeSupport == QuotaInodeSupport_NO_BLOCKDEVICES)
         {
            return QuotaInodeSupport_SOME_BLOCKDEVICES;
         }
         break;
      case QuotaBlockDeviceFsType_ZFS:
         if(currentQuotaInodeSupport == QuotaInodeSupport_UNKNOWN)
         {
            return QuotaInodeSupport_NO_BLOCKDEVICES;
         }
         else if(currentQuotaInodeSupport == QuotaInodeSupport_ALL_BLOCKDEVICES)
         {
            return QuotaInodeSupport_SOME_BLOCKDEVICES;
         }
         break;
      default:
            return currentQuotaInodeSupport;
         break;
   }

   return QuotaInodeSupport_UNKNOWN;
}


QuotaInodeSupport QuotaBlockDevice::quotaInodeSupportFromBlockDevice()
{
   switch(fsType)
   {
      case QuotaBlockDeviceFsType_EXTX: //no break because both FS requires the same handling
      case QuotaBlockDeviceFsType_XFS:
         return QuotaInodeSupport_ALL_BLOCKDEVICES;
         break;
      case QuotaBlockDeviceFsType_ZFS:
         return QuotaInodeSupport_ALL_BLOCKDEVICES;
         break;
      case QuotaBlockDeviceFsType_ZFSOLD:
         return QuotaInodeSupport_NO_BLOCKDEVICES;
         break;
      default:
         return QuotaInodeSupport_UNKNOWN;
         break;
   }
}

QuotaBlockDeviceFsType QuotaBlockDevice::getFsType(const std::string& path)
{
   struct statfs stats;
   const auto result = statfs(path.c_str(), &stats);

   if (result != 0)
   {
      const auto err = errno;
      throw std::runtime_error("Error getting FS Type of " + path + ":" + strerror(err));
   }

   switch (stats.f_type)
   {
   case 0xef53:     // same magic for EXT2/3, from linux/magic.h (not available in older distros)
      return QuotaBlockDeviceFsType_EXTX;
   case 0x58465342: // from statfs man page
      return QuotaBlockDeviceFsType_XFS;
   case 0x2fc12fc1: // found by running statfs on a zfs
      return QuotaBlockDeviceFsType_ZFS;
   }

   return QuotaBlockDeviceFsType_UNKNOWN;
}
