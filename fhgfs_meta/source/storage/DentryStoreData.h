/*
 * Dentry information stored on disk
 */

#ifndef DENTRYSTOREDATA_H_
#define DENTRYSTOREDATA_H_


#include <common/storage/StorageDefinitions.h>

/* Note: Don't forget to update DiskMetaData::getSupportedDentryFeatureFlags() if you add new
 *       flags here. */

// Feature flags, 16 bit
#define DENTRY_FEATURE_INODE_INLINE        1 // inode inlined into a dentry
#define DENTRY_FEATURE_IS_FILEINODE        2 // file-inode
#define DENTRY_FEATURE_MIRRORED            4 // feature flag for mirrored dentries (deprecated)
#define DENTRY_FEATURE_BUDDYMIRRORED       8 // feature flag to indicate buddy mirrored dentries
#define DENTRY_FEATURE_32BITIDS            16 // dentry uses 32bit node IDs on disk

class DentryStoreData
{
   friend class DirEntry;
   friend class DiskMetaData;
   friend class FileInode;

   public:

      DentryStoreData()
          : entryType(DirEntryType_INVALID),
            ownerNodeID(0),
            dentryFeatureFlags(0)
      { }

      DentryStoreData(const std::string& entryID, DirEntryType entryType, NumNodeID ownerNodeID,
         unsigned dentryFeatureFlags)
          : entryID(entryID),
            entryType(entryType),
            ownerNodeID(ownerNodeID),
            dentryFeatureFlags( (uint16_t)dentryFeatureFlags)
      { }

      std::string entryID;    // a filesystem-wide identifier for this dir
      DirEntryType entryType;
      NumNodeID ownerNodeID;   // 0 means undefined
      uint16_t dentryFeatureFlags;

   protected:

      // getters / setters

      void setEntryID(const std::string& entryID)
      {
         this->entryID = entryID;
      }

      const std::string& getEntryID() const
      {
         return this->entryID;
      }

      void setDirEntryType(DirEntryType entryType)
      {
         this->entryType = entryType;
      }

      DirEntryType getDirEntryType() const
      {
         return this->entryType;
      }

      void setOwnerNodeID(NumNodeID ownerNodeID)
      {
         this->ownerNodeID = ownerNodeID;
      }

      NumNodeID getOwnerNodeID() const
      {
         return this->ownerNodeID;
      }

      void setDentryFeatureFlags(unsigned dentryFeatureFlags)
      {
         this->dentryFeatureFlags = dentryFeatureFlags;
      }

      void addDentryFeatureFlag(unsigned dentryFeatureFlag)
      {
         this->dentryFeatureFlags |= dentryFeatureFlag;
      }

      void removeDentryFeatureFlag(unsigned flag)
      {
         this->dentryFeatureFlags &= ~flag;
      }

   public:
      unsigned getDentryFeatureFlags() const
      {
         return this->dentryFeatureFlags;
      }
};


#endif /* DENTRYSTOREDATA_H_ */
