#ifndef DIRINODE_H_
#define DIRINODE_H_

#include <common/storage/striping/StripePattern.h>
#include <common/storage/Metadata.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/SafeMutexLock.h>
#include <common/storage/StatData.h>
#include <common/Common.h>
#include "DirEntryStore.h"
#include "MetadataEx.h"
#include "InodeFileStore.h"

#define DIRINODE_FEATURE_GAM           1 // inode feature flag to indicate, that GAM info is set
#define DIRINODE_FEATURE_EARLY_SUBDIRS 2 // indicate proper alignment for statData

/**
 * Our inode object, but for directories only. Files are in class FileInode.
 */
class DirInode
{
   friend class MetaStore;
   friend class InodeDirStore;
   friend class DiskMetaData;

   public:
      DirInode(std::string id, int mode, unsigned userID, unsigned groupID,
         uint16_t ownerNodeID, StripePattern& stripePattern);

      DirInode(std::string& id) : id(id), entries(id)
      {
         this->stripePattern = NULL;
         this->exclusive = false;
         this->isLoaded = false;
         this->featureFlags = 0;

         if (Upgrade::updateEntryID(id, this->updatedID) )
            this->initializedUpdatedID = true;
         else
         {
            if (id == META_ROOTDIR_ID_STR || id == META_DISPOSALDIR_ID_STR)
            { // exceptions for root and disposal
               this->updatedID = id;
               this->initializedUpdatedID = true;
            }
            else
               this->initializedUpdatedID = false;
         }
      }

      ~DirInode()
      {
         LOG_DEBUG("Delete DirInode", Log_SPAM, std::string("Deleting inode: ") + this->id);

         SAFE_DELETE_NOSET(stripePattern);
      }

      
      static DirInode* createFromFile(std::string id);
      
      StripePattern* createFileStripePattern(UInt16List* preferredNodes);

      /* FIXME Bernd: Shall we really allow access outside of metaStore, without the metaStore
       *              lock? */

      FhgfsOpsErr listIncremental(int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames, StringList* outNames, int64_t* outNewServerOffset);
      FhgfsOpsErr listIncrementalEx(int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames, ListIncExOutArgs& outArgs);
      FhgfsOpsErr listIDFilesIncremental(int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames, ListIncExOutArgs& outArgs);

      bool exists(std::string entryName);
      DirEntryType getEntryInfo(std::string entryName, EntryInfo* outInfo,
         DentryInodeMeta* outInodeMetaData);
      
      FhgfsOpsErr makeDirEntry(DirEntry* entry);
      FhgfsOpsErr linkFileInodeToDir(std::string& inodePath, std::string &fileName);

      FhgfsOpsErr removeDir(std::string entryName, DirEntry** outDirEntry);
      FhgfsOpsErr unlinkDirEntry(std::string entryName, DirEntry* inEntry, bool unlinkEntryName,
         DirEntry** outEntry);

      bool loadIfNotLoaded(void);

      FhgfsOpsErr refreshMetaInfo();

      // non-inlined getters & setters
      FhgfsOpsErr setOwnerNodeID(std::string entryName, uint16_t ownerNode);

      StripePattern* getStripePatternClone();
      bool setStripePattern(StripePattern& newPattern);

      FhgfsOpsErr getStatData(StatData& outStatData);
      void setStatData(StatData& statData);

      bool setAttrData(int validAttribs, SettableFileAttribs* attribs);

      
   private:
      std::string id; // filesystem-wide unique string
      std::string updatedID; // updated - nodeStrID replaced by nodeNumID
      bool initializedUpdatedID;

      uint16_t ownerNodeID; // empty for unknown owner
      std::string ownerNodeIDStr;

      StripePattern* stripePattern; // is the default for new files and subdirs
      
      std::string parentDirID; // just a hint (not reliable)
      std::string updatedParentDirID;

      uint16_t parentNodeID; // just a hint (not reliable), 0 means undefined
      std::string parentNodeIDStr;

      uint16_t featureFlags;

      bool exclusive; // if set, we do not allow other references

      // StatData
      StatData statData;
      unsigned numSubdirs; // indirectly updated by subdir creation/removal
      unsigned numFiles; // indirectly updated by subfile creation/removal
      
      RWLock rwlock;

      DirEntryStore entries;
      
      /* if not set we have an object that has not read data from disk yet, the dir might not
         even exist on disk */
      bool isLoaded;
      Mutex loadLock; // protects the disk load
      
      InodeFileStore fileStore; /* We must not delete the DirInode as long as this
                                 * InodeFileStore still has entries. Therefore a dir reference
                                 * has to be taken for entry in this InodeFileStore */

      std::string pathToDirInode; // for upgrade tool
      std::string oldHashDirPath; // for upgrade tool

      StripePattern* createFileStripePatternUnlocked(UInt16List* preferredNodes);

      FhgfsOpsErr storeInitialMetaData();
      FhgfsOpsErr storeInitialMetaDataInode(std::string entryID);
      bool storeUpdatedMetaDataBuf(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataBufAsXAttr(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataBufAsContents(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataBufAsContentsInPlace(char* buf, unsigned bufLen);
      bool storeUpdatedMetaData();

      FhgfsOpsErr renameDirEntry(std::string fromName, std::string toName);
      FhgfsOpsErr renameDirEntryUnlocked(std::string fromName, std::string toName);

      static bool removeStoredMetaData(std::string id);
      static bool removeStoredMetaDataDir(std::string& id);
      static bool removeStoredMetaDataFile(std::string& id);

      FhgfsOpsErr makeDirEntryUnlocked(DirEntry* entry, bool deleteEntry);
      FhgfsOpsErr linkFileInodeToDirUnlocked(std::string& inodePath, std::string &fileName);

      FhgfsOpsErr removeDirUnlocked(std::string entryName, DirEntry** outDirEntry);
      FhgfsOpsErr unlinkDirEntryUnlocked(std::string entryName, DirEntry* inEntry,
         bool unlinkEntryName, DirEntry** outEntry);
      
      FhgfsOpsErr refreshSubentryCountUnlocked();

      bool loadFromFile();
      bool loadFromFileXAttr();
      bool loadFromFileContents();
      bool loadIfNotLoadedUnlocked();

   public:
      // inliners

      
      /**
       * Note: Must be called before any of the mutator methods (otherwise they will fail)
       */
      FhgfsOpsErr storePersistentMetaData()
      {
         return storeInitialMetaData();
      }
      
      /**
       * Unlink the dir-inode file on disk.
       */
      static bool unlinkStoredInode(std::string id)
      {
         return removeStoredMetaData(id);
      }

      /**
       * Note: Intended to be used by fsck only.
       */
      FhgfsOpsErr storeAsReplacementFile(std::string id)
      {
         // note: creates new dir metadata file for non-existing or invalid one => no locking needed

         removeStoredMetaDataFile(id);
         return storeInitialMetaDataInode(id);
      }
      
      /**
       * Get a dentry
       * note: the difference to getDirEntryInfo/getFileEntryInfo is that this works independent
       * of the entry-type
       */
      bool getDentry(std::string entryName, DirEntry& outEntry)
      {
         bool retVal;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         retVal = this->entries.getDentry(entryName, outEntry);

         safeLock.unlock();

         return retVal;
      }

      /**
       * Get the dentry (dir-entry) of a directory
       */
      bool getDirDentry(std::string dirName, DirEntry& outEntry)
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
      bool getFileDentry(std::string fileName, DirEntry& outEntry)
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
      bool getEntryInfo(std::string entryName, EntryInfo& outEntryInfo)
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
      bool getDirEntryInfo(std::string dirName, EntryInfo& outEntryInfo)
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
      bool getFileEntryInfo(std::string fileName, EntryInfo& outEntryInfo)
      {
         bool retVal;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         retVal = entries.getFileEntryInfo(fileName, outEntryInfo);

         safeLock.unlock();

         return retVal;
      }


      // getters & setters
      std::string getID()
      {
         return id;
      }

      uint16_t getOwnerNodeID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         uint16_t owner = ownerNodeID;

         safeLock.unlock();
         
         return owner;
      }
      
      bool setOwnerNodeID(uint16_t newOwner)
      {
         bool success = true;
         
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);
         
         bool loadSuccess = loadIfNotLoadedUnlocked();
         if (!loadSuccess)
         {
            safeLock.unlock();
            return false;
         }

         uint16_t oldOwner = this->ownerNodeID;
         this->ownerNodeID = newOwner;
         
         if(!storeUpdatedMetaData() )
         { // failed to update metadata => restore old value
            this->ownerNodeID = oldOwner;
            
            success = false;
         }

         safeLock.unlock();
         
         return success;
      }
      
      void getParentInfo(std::string* outParentDirID, uint16_t* outParentNodeID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         *outParentDirID = this->parentDirID;
         *outParentNodeID = this->parentNodeID;

         safeLock.unlock();
      }

      /**
       * Note: Initial means for newly created objects (=> unlocked, unpersistent)
       */
      void setParentInfoInitial(std::string parentDirID, uint16_t parentNodeID)
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

      unsigned getFeatureFlags()
      {
         return this->featureFlags;
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

      
      static FhgfsOpsErr getStatData(std::string dirID, StatData& outStatData)
      {
         DirInode dir(dirID);
         if(!dir.loadFromFile() )
            return FhgfsOpsErr_PATHNOTEXISTS;

         return dir.getStatData(outStatData);
      }

      /**
       * Return create a DirEntry for the given file name
       *
       * note: this->rwlock already needs to be locked
       */
      DirEntry* dirEntryCreateFromFileUnlocked(std::string entryName)
      {
         return this->entries.dirEntryCreateFromFile(entryName);
      }

      StripePattern* getStripePattern() const
      {
         return stripePattern;
      }
      
      /**
       * Move a dentry that has in inlined inode to the inode-hash directories
       * (probably an unlink() of an opened file).
       *
       * @param unlinkFileName Also unlink the fileName or only the ID file.
       */
      bool unlinkBusyFileUnlocked(std::string fileName, DirEntry* dentry, bool unlinkEntryName)
      {
         const char* logContext = "unlink busy file";
         FhgfsOpsErr unlinkRes = this->entries.removeBusyFile(fileName, dentry, unlinkEntryName);


         if (unlinkRes == FhgfsOpsErr_SUCCESS)
         {
            if(unlikely(!decreaseNumFilesAndStoreOnDisk() ) )
               unlinkRes = FhgfsOpsErr_INTERNAL;
         }
         else
         {
           std::string msg = std::string("Failed to remove entry. DirInode") + this->id +
              "entryName: '" + fileName + "entryID: " + dentry->getID() +
              "Error: " + FhgfsOpsErrTk::toErrString(unlinkRes) ;
           LogContext(logContext).logErr(msg);
         }

         return unlinkRes;
      }


      /**
       * Map string IDs to numeric IDs (stripePattern, ownerNodeID, parentNodeID)
       */
      bool mapStringIDs(StringUnsignedMap* targetIdMap, StringUnsignedMap* metaIdMap)
      {
         if (!this->stripePattern->mapStringIDs(targetIdMap) )
            return false;

         if (!Upgrade::mapStringToNumID(metaIdMap, this->ownerNodeIDStr, this->ownerNodeID) )
         {
            if (!this->parentNodeIDStr.empty() ) // might be empty if it is 'disposal'
               return false;
         }

         if (!Upgrade::mapStringToNumID(metaIdMap, this->parentNodeIDStr, this->parentNodeID) )
         {
            if (!this->parentNodeIDStr.empty() ) // might be empty if it is 'root' or 'disposal'
               return false;
         }

         return true;
      }

      const char* getPathToDirInode()
      {
         return this->pathToDirInode.c_str();
      }

      std::string getOldHashDirPath()
      {
         return this->oldHashDirPath;
      }



   private:

      bool updateTimeStampsAndStoreToDisk(const char* logContext)
      {
         int64_t nowSecs = TimeAbs().getTimeval()->tv_sec;;
         this->statData.setAttribChangeTimeSecs(nowSecs);
         this->statData.setModificationTimeSecs(nowSecs);

         if(unlikely(!storeUpdatedMetaData() ) )
         {
            LogContext(logContext).logErr(std::string("Failed to update dir-info on disk: "
               "Dir-ID: ") + this->getID() + std::string(". SysErr: ") + System::getErrString() );

            return false;
         }

         return true;
      }


      bool increaseNumSubDirsAndStoreOnDisk(void)
      {
         // No-op for the upgrade tool, we would double numSubdirs
         return true;
      }

      bool increaseNumFilesAndStoreOnDisk(void)
      {
         // No-op for the upgrade tool, we would double numFiles
         return true;
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
