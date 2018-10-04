#ifndef INODEDIRSTORE_H_
#define INODEDIRSTORE_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/AtomicObjectReferencer.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/Random.h>
#include <common/storage/StatData.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>

class DirInode;

typedef AtomicObjectReferencer<DirInode*> DirectoryReferencer;
typedef std::map<std::string, DirectoryReferencer*> DirectoryMap;
typedef DirectoryMap::iterator DirectoryMapIter;
typedef DirectoryMap::const_iterator DirectoryMapCIter;
typedef DirectoryMap::value_type DirectoryMapVal;

typedef std::map<std::string, DirInode*> DirCacheMap; // keys are dirIDs (same as DirMap)
typedef DirCacheMap::iterator DirCacheMapIter;
typedef DirCacheMap::const_iterator DirCacheMapCIter;
typedef DirCacheMap::value_type DirCacheMapVal;
typedef DirCacheMap::size_type DirCacheMapSizeT;

/**
 * Layer in between our inodes and the data on the underlying file system. So we read/write from/to
 * underlying files and this class is to do this corresponding data access.
 * This object is used for for _directories_ only.
 */
class InodeDirStore
{
   friend class DirInode;
   friend class MetaStore;

   public:
      InodeDirStore();

      ~InodeDirStore()
      {
         this->clearStoreUnlocked();
      };

      InodeDirStore(const InodeDirStore&) = delete;
      InodeDirStore(InodeDirStore&&) = delete;

      InodeDirStore& operator=(const InodeDirStore&) = delete;
      InodeDirStore& operator=(InodeDirStore&&) = delete;

      bool dirInodeInStoreUnlocked(const std::string& dirID);
      DirInode* referenceDirInode(const std::string& dirID, bool isBuddyMirrored, bool forceLoad);
      void releaseDir(const std::string& dirID);
      FhgfsOpsErr removeDirInode(const std::string& dirID, bool isBuddyMirrored);

      size_t getSize();
      size_t getCacheSize();

      FhgfsOpsErr stat(const std::string& dirID, bool isBuddyMirrored, StatData& outStatData,
         NumNodeID* outParentNodeID, std::string* outParentEntryID);
      FhgfsOpsErr setAttr(const std::string& dirID, bool isBuddyMirrored, int validAttribs,
         SettableFileAttribs* attribs);

      void invalidateMirroredDirInodes();

      bool cacheSweepAsync();


   private:
      DirectoryMap dirs;

      size_t refCacheSyncLimit; // synchronous access limit (=> async limit plus some grace size)
      size_t refCacheAsyncLimit; // asynchronous cleanup limit (this is what the user configures)
      Random randGen; // for random cache removal
      DirCacheMap refCache;

      RWLock rwlock;

      void releaseDirUnlocked(const std::string& dirID);

      FhgfsOpsErr isRemovableUnlocked(const std::string& dirID, bool isBuddyMirrored);

      DirectoryMapIter insertDirInodeUnlocked(const std::string& dirID, bool isBuddyMirrored,
         bool forceLoad);

      FhgfsOpsErr setDirParent(EntryInfo* entryInfo, uint16_t parentNodeID);

      void clearStoreUnlocked();

      void cacheAddUnlocked(const std::string& dirID, DirectoryReferencer* dirRefer);
      void cacheRemoveUnlocked(const std::string& dirID);
      void cacheRemoveAllUnlocked();
      bool cacheSweepUnlocked(bool isSyncSweep);
};

#endif /*INODEDIRSTORE_H_*/
