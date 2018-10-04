#ifndef DIRINODE_H_
#define DIRINODE_H_

#include <common/storage/striping/StripePattern.h>
#include <common/storage/Metadata.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/UniqueRWLock.h>
#include <common/storage/StatData.h>
#include <common/Common.h>
#include "DirEntryStore.h"
#include "MetadataEx.h"
#include "InodeFileStore.h"


/* Note: Don't forget to update DiskMetaData::getSupportedDirInodeFeatureFlags() if you add new
 *       flags here. */

#define DIRINODE_FEATURE_EARLY_SUBDIRS  2 // indicate proper alignment for statData
#define DIRINODE_FEATURE_MIRRORED       4 // indicate old mirrored directory (for compatibility)
#define DIRINODE_FEATURE_STATFLAGS      8 // StatData have a flags field
#define DIRINODE_FEATURE_BUDDYMIRRORED  16 // indicate buddy mirrored directory

// limit number of stripes per file to a high but safe number. too many stripe targets will cause
// the serialized stripe pattern to be too large to store reliably, so choose a value well below
// that limit (but still high enough to not bother anyone).
// the number of stripe targets is limited by:
//  * stripe patterns in inodes: for each target, 32 bits of target ID are stored
//  * chunk size infos: for each target, 64 bits of #used_blocks may be stored
// at 256 targets, those two structures may use up to 3072 bytes, leaving ample room for other
// inode data.
#define DIRINODE_MAX_STRIPE_TARGETS  256

/**
 * Our inode object, but for directories only. Files are in class FileInode.
 */
class DirInode
{
   friend class MetaStore;
   friend class InodeDirStore;
   friend class DiskMetaData;

   public:
      DirInode(const std::string& id, int mode, unsigned userID, unsigned groupID,
         NumNodeID ownerNodeID, const StripePattern& stripePattern, bool isBuddyMirrored);

      /**
       * Constructur used to load inodes from disk
       * Note: Not all values are set, as we load those from disk.
       */
      DirInode(const std::string& id, bool isBuddyMirrored)
       : id(id),
         stripePattern(NULL),
         featureFlags(isBuddyMirrored ? DIRINODE_FEATURE_BUDDYMIRRORED : 0),
         exclusive(false),
         entries(id, isBuddyMirrored),
         isLoaded(false)
      { }

      ~DirInode()
      {
         LOG_DEBUG("Delete DirInode", Log_SPAM, std::string("Deleting inode: ") + this->id);

         SAFE_DELETE_NOSET(stripePattern);
      }


      static DirInode* createFromFile(const std::string& id, bool isBuddyMirrored);

      StripePattern* createFileStripePattern(const UInt16List* preferredTargets,
         unsigned numtargets, unsigned chunksize, StoragePoolId storagePoolId);

      FhgfsOpsErr listIncremental(int64_t serverOffset,
         unsigned maxOutNames, StringList* outNames, int64_t* outNewServerOffset);
      FhgfsOpsErr listIncrementalEx(int64_t serverOffset,
         unsigned maxOutNames, bool filterDots, ListIncExOutArgs& outArgs);
      FhgfsOpsErr listIDFilesIncremental(int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames, ListIncExOutArgs& outArgs);

      bool exists(const std::string& entryName);

      FhgfsOpsErr makeDirEntry(DirEntry& entry);
      FhgfsOpsErr linkFileInodeToDir(const std::string& inodePath, const std::string& fileName);

      FhgfsOpsErr removeDir(const std::string& entryName, DirEntry** outDirEntry);
      FhgfsOpsErr unlinkDirEntry(const std::string& entryName, DirEntry* entry,
         unsigned unlinkTypeFlags);

      bool loadIfNotLoaded(void);
      void invalidate();

      FhgfsOpsErr refreshMetaInfo();

      // non-inlined getters & setters
      FhgfsOpsErr setOwnerNodeID(const std::string& entryName, NumNodeID ownerNode);

      StripePattern* getStripePatternClone();
      FhgfsOpsErr setStripePattern(const StripePattern& newPattern, uint32_t actorUID = 0);

      FhgfsOpsErr getStatData(StatData& outStatData,
         NumNodeID* outParentNodeID = NULL, std::string* outParentEntryID = NULL);
      void setStatData(StatData& statData);

      bool setAttrData(int validAttribs, SettableFileAttribs* attribs);
      FhgfsOpsErr setDirParentAndChangeTime(EntryInfo* entryInfo, NumNodeID parentNodeID);

      std::pair<FhgfsOpsErr, StringVector> listXAttr(const EntryInfo* file);
      std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> getXAttr(const EntryInfo* file,
         const std::string& xAttrName, size_t maxSize);
      FhgfsOpsErr removeXAttr(EntryInfo* file, const std::string& xAttrName);
      FhgfsOpsErr setXAttr(EntryInfo* file, const std::string& xAttrName,
            const CharVector& xAttrValue, int flags, bool updateTimestamps = true);

   private:
      std::string id; // filesystem-wide unique string
      NumNodeID ownerNodeID; // 0 means undefined
      StripePattern* stripePattern; // is the default for new files and subdirs

      std::string parentDirID; // must be reliable for NFS
      NumNodeID parentNodeID;   // must be reliable for NFS

      uint16_t featureFlags;

      bool exclusive; // if set, we do not allow other references

      // StatData
      StatData statData;
      uint32_t numSubdirs; // indirectly updated by subdir creation/removal
      uint32_t numFiles; // indirectly updated by subfile creation/removal

      RWLock rwlock;

      DirEntryStore entries;

      /* if not set we have an object that has not read data from disk yet, the dir might not
         even exist on disk */
      bool isLoaded;
      Mutex loadLock; // protects the disk load

      InodeFileStore fileStore; /* We must not delete the DirInode as long as this
                                 * InodeFileStore still has entries. Therefore a dir reference
                                 * has to be taken for entry in this InodeFileStore */

      StripePattern* createFileStripePatternUnlocked(const UInt16List* preferredTargets,
         unsigned numtargets, unsigned chunksize, StoragePoolId storagePoolId);

      FhgfsOpsErr storeInitialMetaData();
      FhgfsOpsErr storeInitialMetaData(const CharVector& defaultACLXAttr,
         const CharVector& accessACLXAttr);
      FhgfsOpsErr storeInitialMetaDataInode();
      bool storeUpdatedMetaDataBuf(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataBufAsXAttr(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataBufAsContents(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataBufAsContentsInPlace(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataUnlocked();

      FhgfsOpsErr renameDirEntry(const std::string& fromName, const std::string& toName,
         DirEntry* overWriteEntry);
      FhgfsOpsErr renameDirEntryUnlocked(const std::string& fromName, const std::string& toName,
         DirEntry* overWriteEntry);

      static bool removeStoredMetaData(const std::string& id);
      static bool removeStoredMetaDataDir(const std::string& id);
      static bool removeStoredMetaDataFile(const std::string& id, bool isBuddyMirrored);

      FhgfsOpsErr refreshMetaInfoUnlocked();


      FhgfsOpsErr makeDirEntryUnlocked(DirEntry* entry);
      FhgfsOpsErr linkFileInodeToDirUnlocked(const std::string& inodePath,
         const std::string &fileName);

      FhgfsOpsErr removeDirUnlocked(const std::string& entryName, DirEntry** outDirEntry);
      FhgfsOpsErr unlinkDirEntryUnlocked(const std::string& entryName, DirEntry* inEntry,
         unsigned unlinkTypeFlags);

      FhgfsOpsErr refreshSubentryCountUnlocked();

      bool loadFromFile();
      bool loadFromFileXAttr();
      bool loadFromFileContents();
      bool loadIfNotLoadedUnlocked();

      FhgfsOpsErr getEntryData(const std::string& entryName, EntryInfo* outInfo,
         FileInodeStoreData* outInodeMetaData);

      bool setAttrDataUnlocked(int validAttribs, SettableFileAttribs* attribs);
      FhgfsOpsErr setDirParentAndChangeTimeUnlocked(EntryInfo* entryInfo, NumNodeID parentNodeID);

      bool unlinkBusyFileUnlocked(const std::string& fileName, DirEntry* dentry,
         unsigned unlinkTypeFlags);

   protected:
      FhgfsOpsErr linkFilesInDirUnlocked(const std::string& fromName, FileInode& fromInode,
         const std::string& toName);

      FhgfsOpsErr setIsBuddyMirrored(const bool isBuddyMirrored);


   public:
      // inliners


      /**
       * Note: Must be called before any of the mutator methods (otherwise they will fail)
       */
      FhgfsOpsErr storePersistentMetaData()
      {
         return storeInitialMetaData();
      }

      FhgfsOpsErr storePersistentMetaData(const CharVector& defaultACLXAttr,
         const CharVector& accessACLXAttr)
      {
         return storeInitialMetaData(defaultACLXAttr, accessACLXAttr);
      }

      /**
       * Unlink the dir-inode file on disk.
       * Note: Assumes that the caller already verified that the directory is empty
       */
      static bool unlinkStoredInode(const std::string& id, bool isBuddyMirrored)
      {
         bool dirRes = DirEntryStore::rmDirEntryStoreDir(id, isBuddyMirrored);
         if(!dirRes)
            return dirRes;

         return removeStoredMetaDataFile(id, isBuddyMirrored);
      }

      /**
       * Note: Intended to be used by fsck only.
       */
      FhgfsOpsErr storeAsReplacementFile(const std::string& id)
      {
         // note: creates new dir metadata file for non-existing or invalid one => no locking needed
         removeStoredMetaDataFile(id, this->getIsBuddyMirrored());
         return storeInitialMetaDataInode();
      }

     /**
       * Return create a DirEntry for the given file name
       *
       * note: this->rwlock already needs to be locked
       */
      DirEntry* dirEntryCreateFromFileUnlocked(const std::string& entryName)
      {
         return this->entries.dirEntryCreateFromFile(entryName);
      }


      /**
       * Get a dentry
       * note: the difference to getDirEntryInfo/getFileEntryInfo is that this works independent
       * of the entry-type
       */
      bool getDentry(const std::string& entryName, DirEntry& outEntry)
      {
         bool retVal;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         retVal = getDentryUnlocked(entryName, outEntry);

         safeLock.unlock();

         return retVal;
      }

      /**
       * Get a dentry
       * note: the difference to getDirEntryInfo/getFileEntryInfo is that this works independent
       * of the entry-type
       */
      bool getDentryUnlocked(const std::string& entryName, DirEntry& outEntry)
      {
         return this->entries.getDentry(entryName, outEntry);
      }

      /**
       * Get the dentry (dir-entry) of a directory
       */
      bool getDirDentry(const std::string& dirName, DirEntry& outEntry)
      {
         bool retVal;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         retVal = entries.getDirDentry(dirName, outEntry);

         safeLock.unlock();

         return retVal;
      }

      /*
       * Get the dentry (dir-entry) of a file
       */
      bool getFileDentry(const std::string& fileName, DirEntry& outEntry)
      {
         bool retVal;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         retVal = entries.getFileDentry(fileName, outEntry);

         safeLock.unlock();

         return retVal;
      }

      /**
       * Get the EntryInfo
       * note: the difference to getDirEntryInfo/getFileEntryInfo is that this works independent
       * of the entry-type
       */
      bool getEntryInfo(const std::string& entryName, EntryInfo& outEntryInfo)
      {

         bool retVal;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         retVal = this->entries.getEntryInfo(entryName, outEntryInfo);

         safeLock.unlock();

         return retVal;
      }

      /**
       * Get the EntryInfo of a directory
       */
      bool getDirEntryInfo(const std::string& dirName, EntryInfo& outEntryInfo)
      {
         bool retVal;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         retVal = entries.getDirEntryInfo(dirName, outEntryInfo);

         safeLock.unlock();

         return retVal;
      }

      /*
       * Get the dir-entry of a file
       */
      bool getFileEntryInfo(const std::string& fileName, EntryInfo& outEntryInfo)
      {
         bool retVal;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         retVal = entries.getFileEntryInfo(fileName, outEntryInfo);

         safeLock.unlock();

         return retVal;
      }

      const std::string& getID() const
      {
         return id;
      }

      NumNodeID getOwnerNodeID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         NumNodeID owner = ownerNodeID;

         safeLock.unlock();

         return owner;
      }

      bool setOwnerNodeID(NumNodeID newOwner)
      {
         bool success = true;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         bool loadSuccess = loadIfNotLoadedUnlocked();
         if (!loadSuccess)
         {
            safeLock.unlock();
            return false;
         }

         NumNodeID oldOwner = this->ownerNodeID;
         this->ownerNodeID = newOwner;

         if(!storeUpdatedMetaDataUnlocked() )
         { // failed to update metadata => restore old value
            this->ownerNodeID = oldOwner;

            success = false;
         }

         safeLock.unlock();

         return success;
      }

      void getParentInfo(std::string* outParentDirID, NumNodeID* outParentNodeID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         *outParentDirID = this->parentDirID;
         *outParentNodeID = this->parentNodeID;

         safeLock.unlock();
      }

      /**
       * Note: Initial means for newly created objects (=> unlocked, unpersistent)
       */
      void setParentInfoInitial(const std::string& parentDirID, NumNodeID parentNodeID)
      {
         this->parentDirID = parentDirID;
         this->parentNodeID = parentNodeID;
      }

      void setFeatureFlags(unsigned flags)
      {
         this->featureFlags = flags;
      }

      void addFeatureFlag(unsigned flag)
      {
         this->featureFlags |= flag;
      }

      void removeFeatureFlag(unsigned flag)
      {
         this->featureFlags &= ~flag;
      }

      unsigned getFeatureFlags() const
      {
         return this->featureFlags;
      }

      void setIsBuddyMirroredFlag(const bool isBuddyMirrored)
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

         if (isBuddyMirrored)
            featureFlags |= DIRINODE_FEATURE_BUDDYMIRRORED;
         else
            featureFlags &= ~DIRINODE_FEATURE_BUDDYMIRRORED;

         entries.setParentID(entries.getParentEntryID(), isBuddyMirrored);
      }

      FhgfsOpsErr setAndStoreIsBuddyMirrored(bool isBuddyMirrored)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         const FhgfsOpsErr result = setIsBuddyMirrored(isBuddyMirrored);

         if (result == FhgfsOpsErr_SUCCESS)
            storeUpdatedMetaDataUnlocked();

         safeLock.unlock();

         return result;
      }

      bool getIsBuddyMirrored() const
      {
         return (getFeatureFlags() & DIRINODE_FEATURE_BUDDYMIRRORED);
      }

      bool getExclusive()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         bool retVal = this->exclusive;

         safeLock.unlock();

         return retVal;
      }

      void setExclusive(bool exclusive)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->exclusive = exclusive;

         safeLock.unlock();
      }

      unsigned getUserID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned retVal = this->statData.getUserID();

         safeLock.unlock();

         return retVal;
      }

      unsigned getUserIDUnlocked()
      {
         return this->statData.getUserID();
      }

      unsigned getGroupID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned retVal = this->statData.getGroupID();

         safeLock.unlock();

         return retVal;
      }

      int getMode()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         int retVal = this->statData.getMode();

         safeLock.unlock();

         return retVal;
      }

      size_t getNumSubEntries()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         size_t retVal = numSubdirs + numFiles;

         safeLock.unlock();

         return retVal;
      }

      bool getIsLoaded()
      {
         UniqueRWLock lock(rwlock, SafeRWLock_READ);
         return isLoaded;
      }


      static FhgfsOpsErr getStatData(const std::string& dirID, bool isBuddyMirrored,
         StatData& outStatData, NumNodeID* outParentNodeID, std::string* outParentEntryID)
      {
         DirInode dir(dirID, isBuddyMirrored);
         if(!dir.loadFromFile() )
            return FhgfsOpsErr_PATHNOTEXISTS;

         return dir.getStatData(outStatData, outParentNodeID, outParentEntryID);
      }

      StripePattern* getStripePattern() const
      {
         return stripePattern;
      }

   private:

      bool updateTimeStampsAndStoreToDisk(const char* logContext)
      {
         int64_t nowSecs = TimeAbs().getTimeval()->tv_sec;;
         this->statData.setAttribChangeTimeSecs(nowSecs);
         this->statData.setModificationTimeSecs(nowSecs);

         if(unlikely(!storeUpdatedMetaDataUnlocked() ) )
         {
            LogContext(logContext).logErr(std::string("Failed to update dir-info on disk: "
               "Dir-ID: ") + this->getID() + std::string(". SysErr: ") + System::getErrString() );

            return false;
         }

         return true;
      }


      bool increaseNumSubDirsAndStoreOnDisk(void)
      {
         const char* logContext = "DirInfo update: increase number of SubDirs";

         numSubdirs++;

         return updateTimeStampsAndStoreToDisk(logContext);
      }

      bool increaseNumFilesAndStoreOnDisk(void)
      {
         const char* logContext = "DirInfo update: increase number of Files";

         numFiles++;

         return updateTimeStampsAndStoreToDisk(logContext);
      }

      bool decreaseNumFilesAndStoreOnDisk(void)
      {
         const char* logContext = "DirInfo update: decrease number of Files";

         if (numFiles) // make sure it does not get sub-zero
            numFiles--;

         return updateTimeStampsAndStoreToDisk(logContext);
      }

      bool decreaseNumSubDirsAndStoreOnDisk(void)
      {
         const char* logContext = "DirInfo update: decrease number of SubDirs";

         if (numSubdirs) // make sure it does not get sub-zero
            numSubdirs--;

         return updateTimeStampsAndStoreToDisk(logContext);
      }
};

#endif /*DIRINODE_H_*/
