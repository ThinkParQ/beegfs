#ifndef INODEDIRSTORE_H_
#define INODEDIRSTORE_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/ObjectReferencer.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/Random.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>

class DirInode;

typedef ObjectReferencer<DirInode*> DirectoryReferencer;
typedef std::map<std::string, DirectoryReferencer*> DirectoryMap;
typedef DirectoryMap::iterator DirectoryMapIter;
typedef DirectoryMap::const_iterator DirectoryMapCIter;
typedef DirectoryMap::value_type DirectoryMapVal;

typedef std::map<std::string, DirectoryReferencer*> DirCacheMap; // keys are dirIDs (same as DirMap)
typedef DirCacheMap::iterator DirCacheMapIter;
typedef DirCacheMap::const_iterator DirCacheMapCIter;
typedef DirCacheMap::value_type DirCacheMapVal;

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
      
      DirInode* referenceDirInode(std::string dirID, bool forceLoad);
      void releaseDir(std::string dirID);
      FhgfsOpsErr removeDirInode(std::string dirID);
      void removeAllDirs(bool ignoreEmptiness);

      size_t getSize();
      size_t getCacheSize();

      bool exists(std::string dirID);
      FhgfsOpsErr stat(std::string dirID, StatData& outStatData);
      FhgfsOpsErr setAttr(std::string dirID, int validAttribs, SettableFileAttribs* attribs);

      bool cacheSweepAsync();


   private:
      DirectoryMap dirs;

      size_t refCacheSyncLimit; // synchronous access limit (=> async limit plus some grace size)
      size_t refCacheAsyncLimit; // asynchronous cleanup limit (this is what the user configures)
      Random randGen; // for random cache removal
      DirCacheMap refCache;
      
      RWLock rwlock;

      void releaseDirUnlocked(std::string dirID);

      FhgfsOpsErr makeDirInode(DirInode* dir, bool keepInode);
      FhgfsOpsErr makeDirInodeUnlocked(DirInode* dir, bool keepInode);
      FhgfsOpsErr isRemovableUnlocked(std::string dirID);
      FhgfsOpsErr removeDirInodeUnlocked(std::string dirID, DirInode** outRemovedDir);
      bool existsUnlocked(std::string dirID);
      
      bool InsertDirInodeUnlocked(std::string id, DirectoryMapIter& newElemIter, bool forceLoad);
      
      void clearStoreUnlocked();

      void cacheAddUnlocked(std::string& dirID, DirectoryReferencer* dirRefer);
      void cacheRemoveUnlocked(std::string& dirID);
      void cacheRemoveAllUnlocked();
      bool cacheSweepUnlocked(bool isSyncSweep);
};

#endif /*INODEDIRSTORE_H_*/
