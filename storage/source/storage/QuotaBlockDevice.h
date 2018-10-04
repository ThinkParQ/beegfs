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

      static QuotaBlockDevice getBlockDeviceOfTarget(const std::string& targetPath,
            uint16_t targetNumID);
      static void getBlockDevicesOfTargets(TargetPathMap* targetPaths,
         QuotaBlockDeviceMap* outBlockDevices);

      QuotaInodeSupport quotaInodeSupportFromBlockDevice() const;

      static QuotaBlockDeviceFsType getFsType(const std::string& path);


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

      void setFsType(QuotaBlockDeviceFsType fsType)
      {
         this->fsType = fsType;
      }

      bool supportsInodeQuota() const
      {
         return quotaInodeSupportFromBlockDevice() == QuotaInodeSupport_ALL_BLOCKDEVICES;
      }
};

#endif /* QUOTABLOCKDEVICE_H_ */
