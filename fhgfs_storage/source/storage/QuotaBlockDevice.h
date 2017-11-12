#ifndef QUOTABLOCKDEVICE_H_
#define QUOTABLOCKDEVICE_H_

#include <common/Common.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/quota/Quota.h>



class QuotaBlockDevice;           //forward declaration


typedef std::map<uint16_t, QuotaBlockDevice> QuotaBlockDeviceMap;
typedef QuotaBlockDeviceMap::iterator QuotaBlockDeviceMapIter;
typedef QuotaBlockDeviceMap::const_iterator QuotaBlockDeviceMapConstIter;
typedef QuotaBlockDeviceMap::value_type QuotaBlockDeviceMapVal;


class QuotaBlockDevice
{
   public:
      QuotaBlockDevice()
      {
         this->storageTargetPath = "";
         this->mountPath = "";
         this->blockDevicePath = "";
         this->fsType = QuotaBlockDeviceFsType_UNKNOWN;
      }

      QuotaBlockDevice(std::string mountPath, std::string blockDevicePath,
         QuotaBlockDeviceFsType fsType, std::string storageTargetPath)
      {
         this->mountPath = mountPath;
         this->blockDevicePath = blockDevicePath;
         this->fsType = fsType;

         // normalize storage target path
         Path storagePath(storageTargetPath); // to get a normalized path string
         std::string normalizedStoragePath(storagePath.str());
         this->storageTargetPath = normalizedStoragePath;
      }

      static QuotaBlockDevice getBlockDeviceOfTarget(std::string& targetPath, uint16_t targetNumID);
      static void getBlockDevicesOfTargets(TargetPathMap* targetPaths,
         QuotaBlockDeviceMap* outBlockDevices);

      static QuotaInodeSupport updateQuotaInodeSupport(QuotaInodeSupport currentQuotaInodeSupport,
         QuotaBlockDeviceFsType fsTypeBlockDevice);
      QuotaInodeSupport quotaInodeSupportFromBlockDevice();


   private:
      std::string storageTargetPath;
      std::string mountPath;
      std::string blockDevicePath;
      QuotaBlockDeviceFsType fsType;


   public:
      // getter & setter
      std::string getMountPath() const
      {
         return mountPath;
      }

      void setMountPath(std::string mountPath)
      {
         this->mountPath = mountPath;
      }

      std::string getBlockDevicePath() const
      {
         return blockDevicePath;
      }

      void setBlockDevicePath(std::string blockDevicePath)
      {
         this->blockDevicePath = blockDevicePath;
      }

      QuotaBlockDeviceFsType getFsType() const
      {
         return fsType;
      }

      std::string getStorageTargetPath() const
      {
         return storageTargetPath;
      }

      std::string getFsTypeString() const
      {
         if(fsType == QuotaBlockDeviceFsType_EXTX)
            return "QuotaBlockDeviceFsType_EXTX";
         else
         if(fsType == QuotaBlockDeviceFsType_XFS)
            return "QuotaBlockDeviceFsType_XFS";
         else
         if(fsType == QuotaBlockDeviceFsType_ZFS)
            return "QuotaBlockDeviceFsType_ZFS";
         else
            return "QuotaBlockDeviceFsType_UNKNOWN";
      }

      void setFsType(QuotaBlockDeviceFsType fsType)
      {
         this->fsType = fsType;
      }
};

#endif /* QUOTABLOCKDEVICE_H_ */
