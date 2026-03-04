#include <program/Program.h>
#include "GlobalInodeLockStore.h"

/**
 * Helper function to convert LockOperationType to string
 */
const char* GlobalInodeLockStore::opTypeToString(LockOperationType opType)
{
   switch (opType)
   {
      case LockOperationType::CHUNK_REBALANCING:
         return "CHUNK_REBALANCING";
      case LockOperationType::FILE_STATE_UPDATE:
         return "FILE_STATE_UPDATE";
      default:
         return "UNKNOWN";
   }
}

/**
 * Insert file inode into global lock store with associated operation type.
 * If inode already exists with same operation type, allows lock (increases refCount if requested).
 * If inode already exists with different operation type, rejects lock attempt.
 *
 * @param entryInfo        Entry information of the file
 * @param fileEvent        Optional file event for logging
 * @param increaseRefCount Whether to increase reference count after insertion/validation
 * @param opType           Operation type requesting the lock
 *
 * @return FhgfsOpsErr_SUCCESS if lock acquired or refCount increased for same operation type
 *         FhgfsOpsErr_INODELOCKED if already locked by different operation type
 *         FhgfsOpsErr_INTERNAL if inode creation failed
 */
FhgfsOpsErr GlobalInodeLockStore::insertFileInode(EntryInfo* entryInfo, FileEvent* fileEvent,
   bool increaseRefCount, LockOperationType opType)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   std::string entryID = entryInfo->getEntryID();
   GlobalInodeLockMapIter iter = this->inodes.find(entryID);
   App* app = Program::getApp();
   FileEventLogger* eventLogger = app->getFileEventLogger();

   if (iter != this->inodes.end() )
   {
      // Inode already exists in store - validate operation type
      GlobalInodeLockEntry* existingEntry = iter->second;

      if (existingEntry->opType != opType)
      {
         // REJECT: Different operation type trying to lock same inode
         LogContext(__func__).log(Log_WARNING,
            "Cannot lock inode - already locked by different operation type. "
            "EntryID: " + entryID +
            "; Existing opType: " + opTypeToString(existingEntry->opType) +
            "; Requested opType: " + opTypeToString(opType));
         return FhgfsOpsErr_INODELOCKED;
      }

      // ACCEPT: Same operation type - this is valid (multiple refs from same operation)
      LOG_DBG(GENERAL, SPAM,
              "Inode already locked by same operation type. Increasing refCount for that type.",
              ("EntryID", entryID), ("opType", opTypeToString(opType)));
   }
   else
   {
      // New lock - create entry
      LOG_DBG(GENERAL, SPAM, "Insert file inode in GlobalInodeLockStore.",
              ("EntryID", entryID), ("opType", opTypeToString(opType)));

      FileInode* inode = FileInode::createFromEntryInfo(entryInfo);
      if (!inode)
      {
         LogContext(__func__).log(Log_WARNING,
            "Failed to create inode; cannot lock in global lock store. "
            "EntryID: " + entryID + "; opType: " + opTypeToString(opType));
         return FhgfsOpsErr_INTERNAL;
      }

      auto entry = new GlobalInodeLockEntry(new GlobalInodeLockReferencer(inode), opType);
      auto insertResult = this->inodes.insert(GlobalInodeLockMap::value_type(entryID, entry));
      iter = insertResult.first;

      if (eventLogger && fileEvent)
      {
         EventContext eventCtx = makeEventContext(
            entryInfo,
            entryInfo->getParentEntryID(),
            0,  //using root user ID
            "",
            inode ? inode->getNumHardlinks() : 0,
            false  // GlobalInodeLockStore only used on primary
         );
         logEvent(eventLogger, *fileEvent, eventCtx);
      }
   }

   // After validation (either new or matching opType), increase refCount if requested
   if (increaseRefCount)
   {
      increaseInodeRefCountUnlocked(iter);
   }

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Take the write lock and decrease the inode reference count.
 * If we are the only one with a reference, release the inode from the lock store.
 * Validates that the operation type matches.
 *
 * @return true if released successfully, false if inode not found or operation type mismatch
 */
bool GlobalInodeLockStore::releaseFileInode(const std::string& entryID, LockOperationType opType)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   return releaseFileInodeUnlocked(entryID, opType);
}

/**
 * Release file inode lock with operation type validation.
 *
 * Note: SafeRWLock_WRITE lock should be held here
 * @return true if released successfully, false if inode not found or operation type mismatch
 */
bool GlobalInodeLockStore::releaseFileInodeUnlocked(const std::string& entryID, LockOperationType opType)
{
   GlobalInodeLockMapIter iter = this->inodes.find(entryID);

   if (iter == this->inodes.end() )
   {
      LogContext(__func__).log(Log_WARNING,
         "Failed to release lock on inode. Inode not found in lock store. entryID:" + entryID);
      return false;
   }

   GlobalInodeLockEntry* entry = iter->second;

   // Validate operation type matches
   if (entry->opType != opType)
   {
      LogContext(__func__).log(Log_WARNING,
         "Operation type mismatch when releasing inode lock. EntryID: " + entryID +
         "; Expected opType: " + opTypeToString(opType) +
         "; Actual opType: " + opTypeToString(entry->opType));
      return false;
   }

   GlobalInodeLockReferencer* inodeRefer = entry->inodeRefer;
   unsigned refCount = inodeRefer->getRefCount();

   // Decrease the reference counter
   if (refCount > 0) // possible that refCount=0 in case of crash, do not decrease counter in that case
   {
      refCount = decreaseInodeRefCountUnlocked(iter);
   }

   if (!refCount)
   {
      // Dropped last reference => unload Inode and delete entry
      SAFE_DELETE(entry->inodeRefer);
      SAFE_DELETE(entry);
      this->inodes.erase(iter);
   }

   return true;
}

/**
 * @return true if file is in the store, false if it is not
 */
bool GlobalInodeLockStore::lookupFileInode(EntryInfo* entryInfo)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   auto iter = this->inodes.find(entryInfo->getEntryID());
   return (iter != this->inodes.end());
}

/**
 * Note: Remember to call releaseInode() after
 * Get inode object and increase reference counter
 *
 * @return Inode object
 */
FileInode* GlobalInodeLockStore::getFileInode(EntryInfo* entryInfo)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   std::string entryID = entryInfo->getEntryID();
   GlobalInodeLockMapIter iter = this->inodes.find(entryID);

   if (iter != this->inodes.end() )
   {
      FileInode* inode = increaseInodeRefCountUnlocked(iter);
      // reset timer since we take a new reference on the inode
      resetTimeCounterUnlocked(entryID);
      return inode;
   }
   return nullptr;
}

/**
 * Get Inode object without increasing reference counter.
 * Used to obtain the inode if refCount is already increased.
 * For first access to the inode, use getFIleInode() which increases the refCount.
 * @return Inode object
 */
FileInode* GlobalInodeLockStore::getFileInodeUnreferenced(EntryInfo* entryInfo)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   std::string entryID = entryInfo->getEntryID();
   GlobalInodeLockMapIter iter = this->inodes.find(entryID);

   if (iter != this->inodes.end() )
   {
      GlobalInodeLockEntry* entry = iter->second;
      FileInode* inode = entry->inodeRefer->getReferencedObject();
      return inode;
   }
   return nullptr;
}


/**
 * Update lock durations for all locked inodes of a specific operation type and release those that exceed the timeout.
 *
 * This method should be periodically invoked by the parent thread that locks inodes
 * (e.g., ChunkBalancerJob) to update the lock timers for all inodes of the specified operation type.
 * Any inode locked by the specified operation type that remains locked longer than maxTimeLimit (in seconds)
 * is forcefully released from GlobalInodeLockStore to recover from thread crashes or other unexpected failures.
 *
 * @param timeElapsed   Seconds elapsed since the last update
 * @param maxTimeLimit   Maximum allowed lock duration (in seconds) before forced release
 * @param opType         Only update and release locks of this operation type
 */
void GlobalInodeLockStore::updateInodeLockTimesteps(int64_t timeElapsed, unsigned int maxTimeLimit, LockOperationType opType)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   for (GlobalInodeLockMapIter iter = this->inodes.begin(); iter != this->inodes.end();)
   {
      GlobalInodeLockEntry* entry = iter->second;

      // Only process locks of the specified operation type
      if (entry->opType != opType)
      {
         ++iter;
         continue;
      }

      entry->timeElapsed += timeElapsed; //update the timestamp

      if (entry->timeElapsed > maxTimeLimit)  //check if time inode spent locked is higher than the maxTimeLimit value, if yes release lock
      {
         std::string entryID = iter->first;
         LogContext(__func__).log(Log_WARNING, "Releasing lock on inode after timeout. entryID:"
            + entryID + "; opType: " + opTypeToString(opType) +
            "; There may be orphaned chunks in the system, "
            "you may want to perform a filesystem check to ensure all additional chunks are deleted.");

         // Need to advance iterator before releasing since release erases the entry
         auto nextIter = std::next(iter);
         bool releaseRes = releaseFileInodeUnlocked(entryID, opType);
         if (unlikely(!releaseRes))
         {
            LogContext(__func__).logErr(
               "Failed to release global inode lock during timeout cleanup! "
               "EntryID: " + entryID + "; opType: " + opTypeToString(opType));
         }
         iter = nextIter;
      }
      else
      {
         ++iter;
      }
   }
}

void GlobalInodeLockStore::clearLockStore()
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   LOG_DBG(GENERAL, DEBUG, "Releasing all locks on inodes in GlobalInodeLockStore.",
           ("# of loaded entries to be cleared:", std::to_string(inodes.size())));

   if (unlikely(!inodes.empty()))
   {
      LogContext(__func__).log(Log_WARNING, "Releasing locks on all inodes in GlobalInodeLockStore. The store is not empty. "
         "There may be orphaned chunks in the system, you may want to perform a filesystem check to ensure all additional chunks are deleted.");
   }

   for (GlobalInodeLockMapIter iter = this->inodes.begin(); iter != this->inodes.end(); iter++)
   {
      GlobalInodeLockEntry* entry = iter->second;
      SAFE_DELETE(entry->inodeRefer);  // Delete the GlobalInodeLockReferencer (which deletes FileInode)
      SAFE_DELETE(entry);               // Delete the GlobalInodeLockEntry
   }
   this->inodes.clear();
}

/**
 * Clear all locks of a specific operation type from the store.
 * This allows selective cleanup of locks without affecting locks held by other operations.
 *
 * @param opType  Operation type whose locks should be released
 */
void GlobalInodeLockStore::clearLockStoreByOpType(LockOperationType opType)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   // Count locks of this operation type
   uint64_t count = 0;
   for (const auto& entry : inodes)
   {
      if (entry.second->opType == opType)
         count++;
   }

   LOG_DBG(GENERAL, DEBUG, "Releasing locks by operation type in GlobalInodeLockStore.",
           ("opType", opTypeToString(opType)),
           ("# of loaded entries to be cleared", std::to_string(count)));

   if (unlikely(count > 0))
   {
      LogContext(__func__).log(Log_WARNING,
         "Releasing locks on " + std::to_string(count) + " inode(s) for operation type " +
         opTypeToString(opType) + ". The store is not empty. "
         "There may be orphaned chunks in the system, you may want to perform a filesystem "
         "check to ensure all additional chunks are deleted.");
   }

   // Release locks matching the operation type
   for (GlobalInodeLockMapIter iter = this->inodes.begin(); iter != this->inodes.end();)
   {
      GlobalInodeLockEntry* entry = iter->second;

      if (entry->opType == opType)
      {
         // Delete the entry and advance iterator
         SAFE_DELETE(entry->inodeRefer);
         SAFE_DELETE(entry);
         iter = this->inodes.erase(iter);
      }
      else
      {
         ++iter;
      }
   }
}

/**
 * Get total number of locked inodes (all operation types).
 *
 * @return Total number of inodes in the lock store
 */
uint64_t GlobalInodeLockStore::getNumLockedInodes()
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   return (uint64_t)inodes.size();
}

/**
 * Get the number of locked inodes for a specific operation type.
 *
 * @param opType  Operation type to count
 * @return Number of inodes locked by the specified operation type
 */
uint64_t GlobalInodeLockStore::getNumLockedInodes(LockOperationType opType)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   uint64_t count = 0;
   for (const auto& entry : inodes)
   {
      if (entry.second->opType == opType)
         count++;
   }
   return count;
}


/**
 * Decrease the inode reference counter using the given iter.
 *
 * Note: GlobalInodeLockStore needs to be write-locked.
 *
 * @return number of inode references after release()
 */
unsigned GlobalInodeLockStore::decreaseInodeRefCountUnlocked(GlobalInodeLockMapIter& iter)
{
   GlobalInodeLockEntry* entry = iter->second;
   GlobalInodeLockReferencer* inodeRefer = entry->inodeRefer;
   unsigned refCount = (unsigned) inodeRefer->release();

   LogContext(__func__).log(Log_DEBUG, "Release file inode. EntryID:" + iter->first +
      "; Refcount:" + std::to_string(inodeRefer->getRefCount()));

   return refCount;
}

/**
 * Increase the inode reference counter using the given iter.
 *
 * Note: GlobalInodeLockStore needs to be write-locked.
 *
 * @return FileInode object
 */
FileInode* GlobalInodeLockStore::increaseInodeRefCountUnlocked(GlobalInodeLockMapIter& iter)
{
   if (unlikely(iter == this->inodes.end() ) )
      return nullptr;

   GlobalInodeLockEntry* entry = iter->second;
   GlobalInodeLockReferencer* inodeRefer = entry->inodeRefer;
   FileInode* inode = inodeRefer->reference();

   LogContext(__func__).log(Log_DEBUG, "Reference file inode. EntryID:" + iter->first +
      "; Refcount:" + std::to_string(inodeRefer->getRefCount()));

   return inode;
}

/**
 * Reset timer that tracks time that Inode has been locked.
 *
 * Note: GlobalInodeLockStore needs to be write-locked.
 *
 */
bool GlobalInodeLockStore::resetTimeCounterUnlocked(const std::string& entryID)
{
   GlobalInodeLockMapIter iter = this->inodes.find(entryID);
   if (unlikely(iter == this->inodes.end() ) )
   {
      LOG_DBG(GENERAL, DEBUG, "Unable to reset time counter in GlobalInodeLockStore. "
         "Locked file may be released earlier than expected.");

      return false;
   }
   iter->second->timeElapsed = 0;
   return true;
}
