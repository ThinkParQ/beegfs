#include <app/App.h>
#include <common/app/log/LogContext.h>
#include <program/Program.h>
#include "QuotaBlockDevice.h"

#include <sys/stat.h>
#include <mntent.h>
#include <fstab.h>


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
   QuotaBlockDeviceFsType resFsType = QuotaBlockDeviceFsType_UNKNOWN;

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
         std::string type(mntData->mnt_type);

         if ( type.compare("xfs") == 0 )
            resFsType = QuotaBlockDeviceFsType_XFS;
         else
         if ( type.find("ext") == 0 )
            resFsType = QuotaBlockDeviceFsType_EXTX;
         else
         if ( type.compare("zfs") == 0 )
            resFsType = QuotaBlockDeviceFsType_ZFS;
         else
            resFsType = QuotaBlockDeviceFsType_UNKNOWN;
      }
   }

   endmntent(mntFile);

   QuotaBlockDevice blockDevice(resMountPath, resBlockDevicePath, resFsType, targetPath);

   // check if the installed libzfs is compatible with implementation
   if(blockDevice.getFsType() == QuotaBlockDeviceFsType_ZFS)
   {
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
         return QuotaInodeSupport_NO_BLOCKDEVICES;
         break;
      default:
         return QuotaInodeSupport_UNKNOWN;
         break;
   }
}
