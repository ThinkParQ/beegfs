/*
 * Data of a FileInode stored on disk.
 */

#ifndef FILEINODESTOREDATA
#define FILEINODESTOREDATA

#include <common/storage/striping/StripePattern.h>
#include <common/storage/Metadata.h>
#include <common/storage/StatData.h>
#include <common/storage/PathInfo.h>

/* Note: Don't forget to update DiskMetaData::getSupportedFileInodeFeatureFlags() if you add new
 *       flags here. */

#define FILEINODE_FEATURE_MIRRORED            1 // indicate mirrored inodes
#define FILEINODE_FEATURE_BUDDYMIRRORED       8 // indicate mirrored inodes

// note: original parent-id and uid are required for the chunk-path calculation
#define FILEINODE_FEATURE_HAS_ORIG_PARENTID  16 // parent-id was updated
#define FILEINODE_FEATURE_HAS_ORIG_UID       32 // uid was updated
#define FILEINODE_FEATURE_HAS_STATFLAGS      64 // stat-data have their own flags
#define FILEINODE_FEATURE_HAS_VERSIONS      128 // file has a cto version counter

enum FileInodeOrigFeature
{
   FileInodeOrigFeature_UNSET = -1,
   FileInodeOrigFeature_TRUE,
   FileInodeOrigFeature_FALSE
};

/* inode data inlined into a direntry, such as in DIRENTRY_STORAGE_FORMAT_VER3 */
class FileInodeStoreData
{
   friend class FileInode;
   friend class DirEntry;
   friend class DirEntryStore;
   friend class GenericDebugMsgEx;
   friend class LookupIntentMsgEx; // just to avoid to copy two time statData
   friend class RecreateDentriesMsgEx;
   friend class RetrieveDirEntriesMsgEx;
   friend class MetaStore;
   friend class DiskMetaData;

   friend class AdjustChunkPermissionsMsgEx;

   friend class TestSerialization; // for testing

   public:

      FileInodeStoreData()
       : inodeFeatureFlags(FILEINODE_FEATURE_HAS_VERSIONS),
         stripePattern(NULL),
         origFeature(FileInodeOrigFeature_UNSET),
         version(0)
      { }

      FileInodeStoreData(const std::string& entryID, StatData* statData,
         StripePattern* stripePattern, unsigned featureFlags, unsigned origParentUID,
         const std::string& origParentEntryID, FileInodeOrigFeature origFeature)
       : inodeFeatureFlags(featureFlags),
         inodeStatData(*statData),
         entryID(entryID),
         origFeature(origFeature),
         origParentUID(origParentUID),
         origParentEntryID(origParentEntryID),
         version(0)
      {
         this->stripePattern              = stripePattern->clone();

         if ((statData->getUserID() != origParentUID) &&
             (origFeature == FileInodeOrigFeature_TRUE) )
            this->inodeFeatureFlags |= FILEINODE_FEATURE_HAS_ORIG_UID;
      }

      bool operator==(const FileInodeStoreData& second) const;

      bool operator!=(const FileInodeStoreData& other) const { return !(*this == other); }

      /**
       * Used to set the values from those read from disk
       */
      FileInodeStoreData(std::string entryID, FileInodeStoreData* diskData) :
         entryID(entryID)
      {
         // this->entryID        = entryID; // set in initializer
         this->stripePattern = NULL;

         setFileInodeStoreData(diskData);
      }

      ~FileInodeStoreData()
      {
         SAFE_DELETE_NOSET(this->stripePattern);
      }

   private:
      uint32_t inodeFeatureFlags; // feature flags for the inode itself, e.g. for mirroring

      StatData inodeStatData;
      std::string entryID; // filesystem-wide unique string
      StripePattern* stripePattern;

      FileInodeOrigFeature origFeature; // indirectly determined via dentry-version
      uint32_t origParentUID;
      std::string origParentEntryID;

      // version number for CTO cache consistency optimizations
      uint64_t version;

      void getPathInfo(PathInfo* outPathInfo);


   public:
      StatData* getInodeStatData()
      {
         return &this->inodeStatData;
      }

      std::string getEntryID()
      {
         return this->entryID;
      }

      StripePattern* getStripePattern()
      {
         return this->stripePattern;
      }

      uint64_t getVersion() const { return version; }
      void     setVersion(uint64_t version) { this->version = version; }

   protected:

      /**
       * Set all fileInodeStoreData
       *
       * Note: Might update existing values and if these are allocated, such as stripePattern,
       *       these need to be freed first.
       */
      void setFileInodeStoreData(FileInodeStoreData* diskData)
      {
         SAFE_DELETE_NOSET(this->stripePattern);
         this->stripePattern     = diskData->getStripePattern()->clone();

         this->inodeStatData     = *(diskData->getInodeStatData() );
         this->inodeFeatureFlags = diskData->getInodeFeatureFlags();
         this->origFeature       = diskData->origFeature;
         this->origParentUID     = diskData->origParentUID;
         this->origParentEntryID = diskData->origParentEntryID;
         this->version = diskData->version;
      }

      void setInodeFeatureFlags(unsigned flags)
      {
         this->inodeFeatureFlags = flags;
      }

      void addInodeFeatureFlag(unsigned flag)
      {
         this->inodeFeatureFlags |= flag;
      }

      void removeInodeFeatureFlag(unsigned flag)
      {
         this->inodeFeatureFlags &= ~flag;
      }

      void setBuddyMirrorFeatureFlag(bool mirrored)
      {
         if (mirrored)
            addInodeFeatureFlag(FILEINODE_FEATURE_BUDDYMIRRORED);
         else
            removeInodeFeatureFlag(FILEINODE_FEATURE_BUDDYMIRRORED);
      }

      bool getIsBuddyMirrored() const
      {
         return (getInodeFeatureFlags() & FILEINODE_FEATURE_BUDDYMIRRORED);
      }

      void setInodeStatData(StatData& statData)
      {
         this->inodeStatData = statData;
      }

      void setEntryID(const std::string& entryID)
      {
         this->entryID = entryID;
      }

      void setPattern(StripePattern* pattern)
      {
         this->stripePattern = pattern;
      }

      void setOrigUID(unsigned origParentUID)
      {
         this->origParentUID = origParentUID;
      }

      /**
       * Set the origParentEntryID based on the parentDir. Feature flag will not be updated.
       * This is for inodes which are not de-inlined and not renamed between dirs.
       */
      void setDynamicOrigParentEntryID(const std::string& origParentEntryID)
      {
         /* Never overwrite existing data! Callers do not check if they need to set it only we do
          * that here. */
         if (!(this->inodeFeatureFlags & FILEINODE_FEATURE_HAS_ORIG_PARENTID) )
            this->origParentEntryID = origParentEntryID;
      }

      /**
       * Set the origParentEntryID from disk, no feature flag test and
       * feature flag will not be updated.
       * Note: Use this for disk-deserialization.
       */
      void setDiskOrigParentEntryID(const std::string& origParentEntryID)
      {
         this->origParentEntryID = origParentEntryID;
      }

      /**
       * Set the origParentEntryID. Feature flag will be updated, origInformation will be stored
       * to disk.
       * Note: Use this on file renames between dirs and de-inlining.
       */
      void setPersistentOrigParentEntryID(const std::string& origParentEntryID)
      {
         /* Never overwrite existing data! Callers do not check if they need to set it only we do
          * that here. */
         if ( (!(this->inodeFeatureFlags & FILEINODE_FEATURE_HAS_ORIG_PARENTID) ) &&
              (this->getOrigFeature() == FileInodeOrigFeature_TRUE) )
         {
            this->origParentEntryID = origParentEntryID;
            addInodeFeatureFlag(FILEINODE_FEATURE_HAS_ORIG_PARENTID);
         }
      }

      uint32_t getInodeFeatureFlags() const
      {
         return this->inodeFeatureFlags;
      }

      StripePattern* getPattern()
      {
         return this->stripePattern;
      }

      void setOrigFeature(FileInodeOrigFeature value)
      {
         this->origFeature = value;
      }

      FileInodeOrigFeature getOrigFeature() const
      {
         return this->origFeature;
      }

      uint32_t getOrigUID() const
      {
         return this->origParentUID;
      }

      const std::string& getOrigParentEntryID() const
      {
         return this->origParentEntryID;
      }

      void incDecNumHardlinks(int value)
      {
         this->inodeStatData.incDecNumHardLinks(value);
      }

      void setNumHardlinks(unsigned numHardlinks)
      {
         this->inodeStatData.setNumHardLinks(numHardlinks);
      }

      unsigned getNumHardlinks() const
      {
         return this->inodeStatData.getNumHardlinks();
      }

      /**
       * Return the pattern and set the internal pattern to NULL to make sure it does not get
       * deleted on object destruction.
       */
      StripePattern* getStripePatternAndSetToNull()
      {
         StripePattern* pattern = this->stripePattern;
         this->stripePattern = NULL;

         return pattern;
      }

      void setAttribChangeTimeSecs(int64_t attribChangeTimeSecs)
      {
         this->inodeStatData.setAttribChangeTimeSecs(attribChangeTimeSecs);
      }

};


#endif /* FILEINODESTOREDATA */
