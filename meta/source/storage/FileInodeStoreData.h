/*
 * Data of a FileInode stored on disk.
 */

#pragma once

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
#define FILEINODE_FEATURE_HAS_RST           256 // file has remote targets
#define FILEINODE_FEATURE_HAS_STATE_FLAGS   512 // file has state flags (access state + data state)

enum FileInodeOrigFeature
{
   FileInodeOrigFeature_UNSET = -1,
   FileInodeOrigFeature_TRUE,
   FileInodeOrigFeature_FALSE
};

// Access control flags (lower 5 bits)
// NOTE: The naming of access flags might seem counter-intuitive.
//
// - UNLOCKED means no access restrictions are in place
// - READ_LOCK means READ operations are BLOCKED, allowing only write operations.
//   The file is effectively "write-only"
// - WRITE_LOCK means WRITE operations are BLOCKED, allowing only read operations.
//   The file is effectively "read-only"
// - When both READ_LOCK and WRITE_LOCK are set, all access is blocked
namespace AccessFlags {
   constexpr uint8_t UNLOCKED    = 0x00; // No flags set (no restrictions)
   constexpr uint8_t READ_LOCK   = 0x01; // Bit 0 (1 << 0) - Block reads
   constexpr uint8_t WRITE_LOCK  = 0x02; // Bit 1 (1 << 1) - Block writes
   constexpr uint8_t RESERVED3   = 0x04; // Bit 2 (1 << 2) - Reserved for future use
   constexpr uint8_t RESERVED4   = 0x08; // Bit 3 (1 << 3) - Reserved for future use
   constexpr uint8_t RESERVED5   = 0x10; // Bit 4 (1 << 4) - Reserved for future use
}

// Represents data state (HSM application defined) (0-7)
namespace DataStates {
    constexpr uint8_t DATASTATE_AVAILABLE       = 0; // 000 (0) - Default: present on BeeGFS
    constexpr uint8_t DATASTATE_MANUAL_RESTORE  = 1; // 001 (1)
    constexpr uint8_t DATASTATE_AUTO_RESTORE    = 2; // 010 (2)
    constexpr uint8_t DATASTATE_DELAYED_RESTORE = 3; // 011 (3)
    constexpr uint8_t DATASTATE_UNAVAILABLE     = 4; // 100 (4)
    constexpr uint8_t DATASTATE_RESERVED5       = 5; // 101 (5)
    constexpr uint8_t DATASTATE_RESERVED6       = 6; // 110 (6)
    constexpr uint8_t DATASTATE_RESERVED7       = 7; // 111 (7)
}

class FileState {
   public:
      static constexpr uint8_t ACCESS_FLAGS_MASK  = 0x1F;  // 0001 1111 (5 bits)
      static constexpr uint8_t DATA_STATE_MASK    = 0xE0;  // 1110 0000 (3 bits)
      static constexpr uint8_t DATA_STATE_SHIFT   = 5;     // Number of bits to shift

      // Constructor taking a raw byte value
      explicit FileState(uint8_t value = 0) : raw(value) {}

      uint8_t getAccessFlags() const
      {
         return raw & ACCESS_FLAGS_MASK;
      }

      uint8_t getDataState() const
      {
         return (raw & DATA_STATE_MASK) >> DATA_STATE_SHIFT;
      }

      bool isReadLocked() const
      {
         return (raw & AccessFlags::READ_LOCK) != 0;
      }

      bool isWriteLocked() const
      {
         return (raw & AccessFlags::WRITE_LOCK) != 0;
      }

      bool isUnlocked() const
      {
         return (getAccessFlags() == 0);
      }

      bool isFullyLocked() const
      {
         return (isReadLocked() && isWriteLocked());
      }

      uint8_t getRawValue() const { return raw; }

private:
      uint8_t raw; // Raw byte representing access flags + data state of a file
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
         fileVersion(0),
         metaVersion(0),
         rawFileState(0)
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
         fileVersion(0),
         metaVersion(0), rawFileState(0)
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
      uint32_t fileVersion;
      uint32_t metaVersion;

      // raw byte value representing access flags + data state
      uint8_t rawFileState;

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

      uint32_t getFileVersion() const { return fileVersion; }
      uint32_t getMetaVersion() const { return metaVersion; }

      void     setFileVersion(uint32_t fileVersion)
      {
         this->fileVersion = fileVersion;
      }

      void     setMetaVersion(uint32_t metaVersion)
      {
         this->metaVersion = metaVersion;
         setMetaVersionStat(metaVersion);  //update metadata version in StatData
      }

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
         this->fileVersion       = diskData->fileVersion;
         setMetaVersion(diskData->metaVersion);
         this->rawFileState     = diskData->rawFileState;
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

      void setIsRstAvailable(bool available)
      {
         if (available)
            addInodeFeatureFlag(FILEINODE_FEATURE_HAS_RST);
         else
            removeInodeFeatureFlag(FILEINODE_FEATURE_HAS_RST);
      }

      bool getIsRstAvailable() const
      {
         return (getInodeFeatureFlags() & FILEINODE_FEATURE_HAS_RST);
      }

      void setFileState(uint8_t value)
      {
         this->rawFileState = value;
         if (this->rawFileState == 0)
            removeInodeFeatureFlag(FILEINODE_FEATURE_HAS_STATE_FLAGS);
         else
            addInodeFeatureFlag(FILEINODE_FEATURE_HAS_STATE_FLAGS);
      }

      uint8_t getFileState() const
      {
         // If FILEINODE_FEATURE_HAS_STATE_FLAGS is not set,
         // return the default "unlocked + zero data state"
         if (!(inodeFeatureFlags & FILEINODE_FEATURE_HAS_STATE_FLAGS))
         {
            constexpr uint8_t defaultAccessFlags = AccessFlags::UNLOCKED;
            constexpr uint8_t defaultDataState   = 0;

            // Format: [dataState (upper 3 bits) | accessFlags (lower 5 bits)]
            return (defaultAccessFlags & FileState::ACCESS_FLAGS_MASK) |
               ((defaultDataState << FileState::DATA_STATE_SHIFT) & FileState::DATA_STATE_MASK);
         }

         // Return the explicitly set state if feature is supported
         return rawFileState;
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

      void setMetaVersionStat(unsigned metaVersion)
      {
         this->inodeStatData.setMetaVersionStat(metaVersion);
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


