#ifndef FILEINODE_H_
#define FILEINODE_H_

#include <common/storage/striping/StripePattern.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/StatData.h>
#include <common/storage/striping/StripeNodeFileInfo.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>
#include <common/threading/PThread.h>
#include <common/toolkit/TimeAbs.h>
#include <common/app/log/LogContext.h>
#include <common/Common.h>
#include <session/LockingNotifier.h>
#include "Locking.h"
#include "MetadataEx.h"
#include "DiskMetaData.h"

#define FILEINODE_FEATURE_MIRROR_ISPRIMARY    1
#define FILEINODE_FEATURE_MIRROR_ISSECONDARY  2

typedef std::set<EntryLockDetails, EntryLockDetails::MapComparator> EntryLockDetailsSet;
typedef EntryLockDetailsSet::iterator EntryLockDetailsSetIter;
typedef EntryLockDetailsSet::const_iterator EntryLockDetailsSetCIter;

typedef std::list<EntryLockDetails> EntryLockDetailsList;
typedef EntryLockDetailsList::iterator EntryLockDetailsListIter;
typedef EntryLockDetailsList::const_iterator EntryLockDetailsListCIter;

typedef std::set<RangeLockDetails, RangeLockDetails::MapComparatorShared> RangeLockSharedSet;
typedef RangeLockSharedSet::iterator RangeLockSharedSetIter;
typedef RangeLockSharedSet::const_iterator RangeLockSharedSetCIter;

typedef std::set<RangeLockDetails, RangeLockDetails::MapComparatorExcl> RangeLockExclSet;
typedef RangeLockExclSet::iterator RangeLockExclSetIter;
typedef RangeLockExclSet::const_iterator RangeLockExclSetCIter;

typedef std::list<RangeLockDetails> RangeLockDetailsList;
typedef RangeLockDetailsList::iterator RangeLockDetailsListIter;
typedef RangeLockDetailsList::const_iterator RangeLockDetailsListCIter;

/**
 * Data are not really belonging to the inode, but we just need it to write inodes in the
 * dentry-format. We need to read those from disk first or obtain it from the dentry.
 */
struct DentryCompatData
{
      DirEntryType entryType;
      unsigned featureFlags;
};

/**
 * Our inode object, but for files only (so all file types except of directories).
 * Directories are in class DirInode.
 */
class FileInode
{
   friend class InodeFileStore;
   friend class SessionFile; /* (needed for entry/range locking) */


   public:

      /**
       * Should be rarely used and if required, only with an immediate following
       * inode->deserialize(buf)
       */
      FileInode();

      /**
       * The preferred inode initializer.
       */
      FileInode(std::string entryID, StatData* statData, StripePattern& stripePattern,
         DirEntryType entryType, unsigned dentryFeatureFlags, unsigned chunkHash);

      /**
       * NOTE: Avoid to use this initilizer. It is only for not important inodes, such as
       *       fake-inodes if chunk-file unlink did not work.
       */
      FileInode(std::string entryID, int mode, unsigned userID, unsigned groupID,
         StripePattern& stripePattern, DirEntryType entryType, unsigned featureFlags,
         unsigned chunkHash);


      ~FileInode()
         {
            LOG_DEBUG("Delete FileInode", Log_SPAM, std::string("Deleting inode: ") +
               this->entryID);

            SAFE_DELETE_NOSET(stripePattern);
         }

      /* FIXME Bernd: Shall we really allow access outside of metaStore, without the metaStore
       *              lock? */

      void decNumSessionsAndStore(EntryInfo* entryInfo, unsigned accessFlags);
      static FileInode* createFromEntryInfo(EntryInfo* entryInfo);

      unsigned serializeMetaData(char* buf);

      bool flockEntry(EntryLockDetails& lockDetails);
      void flockEntryCancelAllWaiters();
      void flockEntryCancelByClientID(std::string clientID);
      std::string flockEntryGetAllAsStr();
      bool flockRange(RangeLockDetails& lockDetails);
      void flockRangeCancelAllWaiters();
      void flockRangeCancelByClientID(std::string clientID);
      bool flockRangeGetConflictor(RangeLockDetails& lockDetails, RangeLockDetails* outConflictor);
      std::string flockRangeGetAllAsStr();

      bool deserializeMetaData(const char* buf);


   private:
      std::string entryID; // filesystem-wide unique string
      std::string updatedID; // for upgrade tool

      std::string parentEntryID;
      uint16_t parentNodeID;
      unsigned featureFlags;
      bool globalFileStore; /* Boolean if the inode is part of the per-directory or global
                             * InodeFileStore. Usually per-directory, exceptions are:
                             * Inodes with hard-links, unlinked but open inodes and open files
                             * being renamed. If this is true parentDirID and parentNodeID
                             * must not be used.
                             * FIXME Bernd: make sure value is set to true for those */

      StripePattern* stripePattern;
      StripeNodeFileInfoVec fileInfoVec; // stores individual data for each node (e.g. file length),
         // esp. the DYNAMIC ATTRIBS, that live only in memory and are not stored to disk (compared
         // to static attribs)

      unsigned chunkHash; // hash for storage chunks

      bool exclusive; // if set, we don't allow further references

      unsigned numSessionsRead; // open read-only
      unsigned numSessionsWrite; // open for writing or read-write

      // StatData
      StatData statData;

      bool isInlined; // boolean if the inode inlined into the Dentry or a separate file

      // flock() queues (entire file locking, i.e. no ranges)
      EntryLockDetails exclFLock; // current exclusive lock
      EntryLockDetailsSet sharedFLocks; // current shared locks (key is lock, value is dummy)
      EntryLockDetailsList waitersExclFLock; // queue (append new to end, pop from top)
      EntryLockDetailsList waitersSharedFLock; // queue (append new to end, pop from top)
      StringSet waitersLockIDsFLock; // currently enqueued lockIDs (for fast duplicate check)

      // fcntl() flock queues (range-based)
      RangeLockExclSet exclRangeFLocks; // current exclusive locks
      RangeLockSharedSet sharedRangeFLocks;  // current shared locks (key is lock, value is dummy)
      RangeLockDetailsList waitersExclRangeFLock; // queue (append new to end, pop from top)
      RangeLockDetailsList waitersSharedRangeFLock; // queue (append new to end, pop from top)
      StringSet waitersLockIDsRangeFLock; // currently enqueued lockIDs (for fast duplicate check)

      RWLock rwlock;

      DentryCompatData dentryCompatData;

      AtomicSSizeT numParentRefs; /* counter how often parentDir is referenced, 0 if we
                                  * are part of MetaStores FileStore */

      std::string pathToInodeFile; // for upgrade tool
      std::string relInodeFilePath;      // for upgrade tool, (hash) path to inode


      static FileInode* createFromInlinedInode(EntryInfo* entryInfo);
      static FileInode* createFromInodeFile(EntryInfo* entryInfo);

      void initFileInfoVec();
      void updateDynamicAttribs(void);

      bool setAttrData(EntryInfo* entryInfo, int validAttribs, SettableFileAttribs* attribs);

      bool storeUpdatedMetaDataBuf(char* buf, unsigned bufLen, int fd = -1);
      bool storeUpdatedMetaDataBufAsXAttr(char* buf, unsigned bufLen, int fd = -1);
      bool storeUpdatedMetaDataBufAsContents(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataBufAsContentsInPlace(char* buf, unsigned bufLen, int fd = -1);
      bool storeUpdatedInode(EntryInfo* entryInfo, StripePattern* updatedStripePattern = NULL);
      FhgfsOpsErr storeUpdatedInlinedInode(EntryInfo* entryInfo,
         StripePattern* updatedStripePattern = NULL);
      FhgfsOpsErr storeInitialNonInlinedInode();

      static bool removeStoredMetaData(std::string id);

      bool loadFromInodeFile(EntryInfo* entryInfo);
      bool loadFromFileXAttr(const std::string& id);
      bool loadFromFileContents(const std::string& id);

      bool flockEntryUnlocked(EntryLockDetails& lockDetails);
      bool flockEntryCancelByHandle(EntryLockDetails& lockDetails);
      bool flockRangeUnlocked(RangeLockDetails& lockDetails);
      bool flockRangeCancelByHandle(RangeLockDetails& lockDetails);

      bool flockEntryCheckConflicts(EntryLockDetails& lockDetails, EntryLockDetails* outConflictor);
      bool flockEntryIsGranted(EntryLockDetails& lockDetails);
      bool flockEntryUnlock(EntryLockDetails& lockDetails);
      void flockEntryShared(EntryLockDetails& lockDetails);
      void flockEntryExclusive(EntryLockDetails& lockDetails);
      void flockEntryTryNextWaiters();

      bool flockRangeCheckConflicts(RangeLockDetails& lockDetails, RangeLockDetails* outConflictor);
      bool flockRangeCheckConflictsEx(RangeLockDetails& lockDetails, int maxExclWaitersCheckNum,
         RangeLockDetails* outConflictor);
      bool flockRangeIsGranted(RangeLockDetails& lockDetails);
      bool flockRangeUnlock(RangeLockDetails& lockDetails);
      void flockRangeShared(RangeLockDetails& lockDetails);
      void flockRangeExclusive(RangeLockDetails& lockDetails);
      void flockRangeTryNextWaiters();


   public:
      // inliners

      /**
       * Tag the inode as part of the global file store (InodeFileStore) and not per-directory.
       */
      void setIsInGlobalFileStore(void)
      {
         this->globalFileStore = true;
      }

      /**
       * Save meta-data to disk
       */
      bool updateInodeOnDisk(EntryInfo* entryInfo, StripePattern* updatedStripePattern = NULL)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         bool retVal = storeUpdatedInode(entryInfo, updatedStripePattern);

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /**
       * Unlink the stored file-inode file on disk.
       */
      static bool unlinkStoredInode(std::string id)
      {
         return removeStoredMetaData(id);
      }

      // getters & setters
      std::string getEntryID()
      {
         return this->entryID;
      }

      std::string getUpdatedID()
      {
         return this->updatedID;
      }

      /**
       * Note: Does not (immediately) update the persistent metadata.
       * Note: Be careful with this!
       */
      void setIDUnpersistent(std::string newID)
      {
         this->entryID = newID;
      }

      StripePattern* getStripePattern()
      {
         return stripePattern;
      }

      /**
       * Return the stripe pattern and set our value to NULL.
       *
       * Note: Usually this inode should not be further used anymore.
       */
      StripePattern* getStripePatternAndSetToNull()
      {
         StripePattern* stripePattern = this->stripePattern;
         this->stripePattern = NULL;

         return stripePattern;
      }

      void getParentInfo(std::string* outParentDirID, uint16_t* outParentNodeID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         *outParentDirID = this->parentEntryID;
         *outParentNodeID = this->parentNodeID;

         safeLock.unlock();
      }

      /**
       * Note: Initial means for newly created objects (=> unlocked, unpersistent)
       */
      void setParentInfoInitial(std::string parentEntryID, uint16_t parentNodeID)
      {
         this->parentEntryID = parentEntryID;
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


      /**
       * Note: We do not set our class values here yet, because this method is called frequently and
       *       we do not need our class values here yet. So class values are only set on demand.
       *       this->updateDynamicAttribs() will set/update our class values·
       *
       * @param erroneousVec if errors occurred during vector retrieval from storage nodes
       *       (this parameter was used to set dynAttribsUptodate to false, but we do not have
       *        this value anymore since the switch to (post-)DB metadata format)
       * @return false if error during save occurred (but we don't save dynAttribs currently)
       *
       * FIXME Bernd: remove parameter erroneousVec at all
       */
      bool setDynAttribs(DynamicFileAttribsVec& newAttribsVec, bool erroneousVec)
      {
         /* note: we cannot afford to make a disk write each time this is updated, so
            we only update metadata on close currrently */

         bool retVal = true;
         bool anyAttribUpdated = false;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         if(unlikely(newAttribsVec.size() != fileInfoVec.size() ) )
         { // should never happen
            PThread::getCurrentThreadApp()->getLogger()->logErr("Apply dynamic file attribs",
               "Vector sizes do not match - This should never happen!");

            retVal = false;
            goto out;
         }


         for(size_t i=0; i < newAttribsVec.size(); i++)
         {
            if(fileInfoVec[i].updateDynAttribs(newAttribsVec[i] ) )
               anyAttribUpdated = true;
         }

         IGNORE_UNUSED_VARIABLE(anyAttribUpdated);

out:
         safeLock.unlock();

         return retVal;
      }

      /**
       * Note: Use this only if the file is not loaded already (because otherwise the dyn attribs
       * will be outdated)
       */
      static FhgfsOpsErr getStatData(EntryInfo* entryInfo, StatData& outStatData)
      {

         // FIXME Bernd / Sven: Shall we add code for "loadFromEntryInfo()?
         FileInode* inode = createFromEntryInfo(entryInfo);

         if(!inode)
            return FhgfsOpsErr_PATHNOTEXISTS;

         FhgfsOpsErr retVal = inode->getStatData(outStatData);

         delete inode;

         return retVal;
      }

      FhgfsOpsErr getStatDataUnlocked(StatData& outStatData)
      {
         FhgfsOpsErr statRes = FhgfsOpsErr_SUCCESS;

         this->updateDynamicAttribs();

         outStatData = this->statData;

         /* note: we don't check numSessionsRead below (for dyn attribs outated statRes), because
            that would be too much overhead; side-effect is that we don't update atime for read-only
            files while they are open. */

         if(numSessionsWrite)
            statRes = FhgfsOpsErr_DYNAMICATTRIBSOUTDATED;

         return statRes;
      }

      FhgfsOpsErr getStatData(StatData& outStatData)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         FhgfsOpsErr statRes = getStatDataUnlocked(outStatData);

         safeLock.unlock();

         return statRes;
      }

      void setStatData(StatData& statData)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->statData = statData;

         safeLock.unlock();
      }

      /**
       * Note: Does not take a lock and is a very special use case. For example on moving a file
       *       to a remote server, when an inode object is created from a buffer and we want to
       *       have values from this inode object.
       */
      StatData* getNotUpdatedStatDataUseCarefully()
      {
         return &this->statData;
      }

      void setNumHardlinksUnpersistent(unsigned numHardlinks)
      {
         /* note: this is the version for the quick on-close unlink check, so file is referenced by
          *       client sessions and we don't need to update persistent metadata
          */

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->statData.setNumHardLinks(numHardlinks);

         safeLock.unlock();
      }

      unsigned getNumHardlinks()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned retVal = this->statData.getNumHardlinks();

         safeLock.unlock();

         return retVal;
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

      /**
       * @return num read-sessions + num write-sessions
       */
      unsigned getNumSessionsAll()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned num = this->numSessionsRead + this->numSessionsWrite;

         safeLock.unlock();

         return num;
      }

      unsigned getNumSessionsRead()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned num = this->numSessionsRead;

         safeLock.unlock();

         return num;
      }

      unsigned getNumSessionsWrite()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned num = this->numSessionsWrite;

         safeLock.unlock();

         return num;
      }

      /**
       * Increase number of sessions for read or write (=> file open).
       *
       * @param accessFlags OPENFILE_ACCESS_... flags
       */
      void incNumSessions(unsigned accessFlags)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         if(accessFlags & OPENFILE_ACCESS_READ)
            this->numSessionsRead++;
         else
            this->numSessionsWrite++; // (includes read+write)

         safeLock.unlock();
      }

      FileInode* clone()
      {
         char* buf = (char*)malloc(META_SERBUF_SIZE);
         serializeMetaData(buf);

         FileInode *clone = new FileInode();
         bool cloneRes = clone->deserializeMetaData(buf);
         if (unlikely(cloneRes == false) )
         {
            delete clone;
            clone = NULL;
         }

         free(buf);

         return clone;
      }

      void setIsInlined(bool value)
      {
         this->isInlined = value;

         this->dentryCompatData.featureFlags &= ~(DISKMETADATA_FEATURE_INODE_INLINE);
      }

      bool getIsInlined(void)
      {
         return this->isInlined;
      }

      void setChunkHash(unsigned chunkHash)
      {
         this->chunkHash = chunkHash;
      }

      unsigned getChunkHash()
      {
         return this->chunkHash;
      }

      void incParentRef()
      {
         this->numParentRefs.increase();
      }

      void decParentRef()
      {
         this->numParentRefs.decrease();
      }

      ssize_t getNumParentRefs()
      {
         return this->numParentRefs.read();
      }

      bool mapStripePatternStringIDs(StringUnsignedMap* idMap)
      {
         return this->stripePattern->mapStringIDs(idMap);
      }

      std::string getPathToInodeFile()
      {
         return this->pathToInodeFile;
      }

      std::string getReInodeFilePathPath()
      {
         return this->relInodeFilePath;
      }

   private:

      // inliners



};

#endif /*FILEINODE_H_*/
