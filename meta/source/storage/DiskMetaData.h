#ifndef DISKMETADATA_H_
#define DISKMETADATA_H_

#include "FileInodeStoreData.h"

#define DIRENTRY_SERBUF_SIZE     (1024 * 4) /* make sure that this is always smaller or equal to
                                             * META_SERBUF_SIZE */

#define DISKMETADATA_TYPE_BUF_POS     0
#define DIRENTRY_TYPE_BUF_POS         4

// 8-bit
enum DiskMetaDataType
{
   DiskMetaDataType_FILEDENTRY = 1, // may have inlined inodes
   DiskMetaDataType_DIRDENTRY  = 2,
   DiskMetaDataType_FILEINODE  = 3, // currently in dentry-format
   DiskMetaDataType_DIRINODE   = 4,
};


// forward declarations;
class DirInode;
class FileInode;
class DirEntry;
class DentryStoreData;
class FileInodeStoreData;

/**
 * Generic class for on-disk storage.
 *
 * Note: The class is inherited from DirEntry::. But is an object in FileInode:: methods.
 */
class DiskMetaData
{
   public:

      // used for DirEntry derived class
      DiskMetaData(DentryStoreData* dentryData, FileInodeStoreData* inodeData)
      {
         this->dentryDiskData = dentryData;
         this->inodeData   = inodeData;
      };


      void serializeFileInode(Serializer& ser);
      void serializeDentry(Serializer& ser);
      void deserializeFileInode(Deserializer& des);
      void deserializeDentry(Deserializer& des);
      static void serializeDirInode(Serializer& ser, DirInode& inode);
      static void deserializeDirInode(Deserializer& des, DirInode& outInode);

   protected:
      DentryStoreData* dentryDiskData; // Not owned by this object!
      FileInodeStoreData* inodeData;  // Not owned by this object!

   private:
      void serializeInDentryFormat(Serializer& ser, DiskMetaDataType metaDataType);
      void serializeDentryV3(Serializer& ser);
      void serializeDentryV4(Serializer& ser);
      void serializeDentryV6(Serializer& ser);
      void deserializeDentryV3(Deserializer& des);
      void deserializeDentryV4(Deserializer& des);
      void deserializeDentryV5(Deserializer& des);
      void deserializeDentryV6(Deserializer& des);
      void deserializeDentryV5V6(Deserializer& des, bool hasStoragePool);

      static unsigned getSupportedDentryFeatureFlags();
      static unsigned getSupportedDentryV4FileInodeFeatureFlags();
      static unsigned getSupportedDentryV5FileInodeFeatureFlags();
      static unsigned getSupportedDirInodeFeatureFlags();

      static bool checkFeatureFlagsCompat(unsigned usedFeatureFlags,
         unsigned supportedFeatureFlags);

      template<typename Inode, typename Ctx>
      static void serializeDirInodeCommonData(Inode& inode, Ctx& ctx);
};

#endif // DISKMETADATA_H_
