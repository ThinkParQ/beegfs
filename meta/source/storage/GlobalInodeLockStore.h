#pragma once

#include <common/Common.h>
#include <storage/FileInode.h>
#include <components/FileEventLogger.h>

/**
 * Enum to identify the type of operation that acquired an inode lock.
 * This allows selective release of locks by operation type to prevent
 * unintended release of locks held by other operations.
 */
enum class LockOperationType
{
   CHUNK_REBALANCING,    // Chunk balancing operations
   FILE_STATE_UPDATE     // File state update operations
};

typedef ObjectReferencer<FileInode*> GlobalInodeLockReferencer;

/**
 * Wrapper structure that holds the inode lock reference, operation type,
 * and timestamp. This ensures operation type and timestamp are always
 * kept in sync with the lock, eliminating the need for separate maps.
 */
struct GlobalInodeLockEntry
{
   GlobalInodeLockReferencer* inodeRefer;
   LockOperationType opType;
   int64_t timeElapsed;  // Seconds since lock acquisition

   GlobalInodeLockEntry(GlobalInodeLockReferencer* ref, LockOperationType type)
      : inodeRefer(ref), opType(type), timeElapsed(0) {}
};

typedef std::map<std::string, GlobalInodeLockEntry*> GlobalInodeLockMap;
typedef GlobalInodeLockMap::iterator GlobalInodeLockMapIter;

/**
 * Global store for file inodes which are locked for referencing.
 * Used for chunk balancing and other operations that require exclusive
 * inode access. This object initializes the GlobalInodeLockMap with
 * operation type tracking. It is used for taking and releasing locks on
 * the inodes. "locking" here means we successfully insert an element into the map.
 */
class GlobalInodeLockStore
{
   public:
      ~GlobalInodeLockStore()
      {
         this->clearLockStore();
      }

      FhgfsOpsErr insertFileInode(EntryInfo* entryInfo, FileEvent* fileEvent, bool increaseRefCount,
                                  LockOperationType opType);
      bool releaseFileInode(const std::string& entryID, LockOperationType opType);

      bool lookupFileInode(EntryInfo* entryInfo);
      FileInode* getFileInode(EntryInfo* entryInfo);
      FileInode* getFileInodeUnreferenced(EntryInfo* entryInfo);
      void updateInodeLockTimesteps(int64_t timeElapsed, unsigned int maxTimeLimit, LockOperationType opType);

      // Cleanup functions
      void clearLockStore();
      void clearLockStoreByOpType(LockOperationType opType);

      uint64_t getNumLockedInodes();
      uint64_t getNumLockedInodes(LockOperationType opType);

   private:
      GlobalInodeLockMap inodes;
      RWLock rwlock;

      static const char* opTypeToString(LockOperationType opType);

      bool releaseFileInodeUnlocked(const std::string& entryID, LockOperationType opType);
      unsigned decreaseInodeRefCountUnlocked(GlobalInodeLockMapIter& iter);
      FileInode* increaseInodeRefCountUnlocked(GlobalInodeLockMapIter& iter);
      bool resetTimeCounterUnlocked(const std::string& entryID);
};

