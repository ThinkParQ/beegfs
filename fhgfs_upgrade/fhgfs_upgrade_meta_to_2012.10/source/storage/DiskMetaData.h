#ifndef DISKMETADATA_H_
#define DISKMETADATA_H_

// Feature flags, 16 bit
#define DISKMETADATA_FEATURE_INODE_INLINE        1 // inode inlined into a dentry
#define DISKMETADATA_FEATURE_IS_FILEINODE        2 // file-inode, FIXME BErnd: Not required

#define DIRENTRY_SERBUF_SIZE     (1024*4) /* make sure that this is always smaller or equal to
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


class DirInode; // forward declaration; FIXME Bernd: can we avoid a recursive include
class FileInode;
class DirEntry;

/**
 * Generic class to on disk storage.
 *
 * Note: The class is inherited from DirEntry::. But is an object in FileInode:: methods.
 */
class DiskMetaData
{
   public:

      // used for DirEntry derived class
      DiskMetaData()
      {
         this->inodeDataPtr = &this->inodeData;
      };

      // used for DirEntry derived class
      DiskMetaData(DirEntryType entryType, std::string entryID, uint16_t ownerNodeID,
         int featureFlags) :
         entryID(entryID), entryType(entryType), ownerNodeID(ownerNodeID),
         dentryFeatureFlags(featureFlags) {}

      // used for inode serialization
      DiskMetaData(DirEntryType entryType, DentryInodeMeta* diskInodeData) :
         entryID(diskInodeData->getID() )
      {
         this->entryType    = entryType;
         this->ownerNodeID  = 0; // unsed here
         this->dentryFeatureFlags = diskInodeData->getDentryFeatureFlags();

         this->inodeDataPtr = diskInodeData;
      }

      unsigned serializeFileInode(char* buf);
      unsigned serializeDentry(char* buf);
      bool deserializeFileInode(const char* buf);
      bool deserializeDentry(const char* buf);
      static DirEntryType deserializeInode(const char* buf, FileInode** outFileInode,
         DirInode** outDirInode);
      static unsigned serializeDirInode(char* buf, DirInode* inode);
      static bool deserializeDirInode(const char* buf, DirInode* outInode);

   protected:
      std::string entryID;    // a filesystem-wide identifier for this dir
      std::string updatedEntryID;
      DirEntryType entryType;

      std::string ownerNodeIDStr; // for upgrade tool
      uint16_t ownerNodeID;   // 0 means undefined

      uint16_t dentryFeatureFlags;

      DentryInodeMeta inodeData;
      DentryInodeMeta* inodeDataPtr; /* Pointer to inodeData or to an external object.
                                      * Used to avoid another temporary allocation in the
                                      * non-inlined inode serialization part. */

   private:
      unsigned serializeInDentryFormat(char* buf, DiskMetaDataType metaDataType);
      unsigned serializeDentryV3(char* buf);
      unsigned serializeDentryV4(char* buf);
      bool deserializeDentryV2(const char* buf, size_t bufLen, unsigned* outLen);
      bool deserializeDentryV3(const char* buf, size_t bufLen, unsigned* outLen);
      bool deserializeDentryV4(const char* buf, size_t bufLen, unsigned* outLen);


   // inliners

   public:

      DentryInodeMeta* getInodeData(void)
      {
         return &this->inodeData;
      }

      DirEntryType getEntryType(void)
      {
         return this->entryType;
      }

      unsigned getDentryFeatureFlags(void)
      {
         return this->dentryFeatureFlags;
      }
};

#endif // DISKMETADATA_H_
