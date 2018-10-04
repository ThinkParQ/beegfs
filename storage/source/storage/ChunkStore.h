#ifndef CHUNKSTORE_H_
#define CHUNKSTORE_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/AtomicObjectReferencer.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/Random.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>

#include "ChunkDir.h"


#define PATH_DEPTH_IDENTIFIER 'l' // we use 'l' (level) instead of 'd', as d is part of hex numbers


class ChunkDir;

typedef AtomicObjectReferencer<ChunkDir*> ChunkDirReferencer;
typedef std::map<std::string, ChunkDirReferencer*> DirectoryMap;
typedef DirectoryMap::iterator DirectoryMapIter;
typedef DirectoryMap::const_iterator DirectoryMapCIter;
typedef DirectoryMap::value_type DirectoryMapVal;

typedef std::map<std::string, ChunkDirReferencer*> DirCacheMap; // keys are dirIDs (same as DirMap)
typedef DirCacheMap::iterator DirCacheMapIter;
typedef DirCacheMap::const_iterator DirCacheMapCIter;
typedef DirCacheMap::value_type DirCacheMapVal;

/**
 * Layer in between our inodes and the data on the underlying file system. So we read/write from/to
 * underlying files and this class is to do this corresponding data access.
 * This object is used for for _directories_ only.
 */
class ChunkStore
{
   public:
      ChunkStore();

      ~ChunkStore()
      {
         this->clearStoreUnlocked();
      }

      bool dirInStoreUnlocked(std::string dirID);
      ChunkDir* referenceDir(std::string dirID);
      void releaseDir(std::string dirID);

      size_t getCacheSize();

      bool cacheSweepAsync();

      bool rmdirChunkDirPath(int targetFD, Path* chunkDirPath);

      FhgfsOpsErr openChunkFile(int targetFD, const Path* chunkDirPath,
         const std::string& chunkFilePathStr, bool hasOrigFeature, int openFlags, int* outFD,
         const SessionQuotaInfo* quotaInfo, const ExceededQuotaStorePtr exQuotaStore);

      bool chmodV2ChunkDirPath(int targetFD, const Path* chunkDirPath, const std::string& entryID);


   private:
      DirectoryMap dirs;

      size_t refCacheSyncLimit; // synchronous access limit (=> async limit plus some grace size)
      size_t refCacheAsyncLimit; // asynchronous cleanup limit (this is what the user configures)
      Random randGen; // for random cache removal
      DirCacheMap refCache;

      RWLock rwlock;

      void InsertChunkDirUnlocked(std::string dirID, DirectoryMapIter& newElemIter);

      void releaseDirUnlocked(std::string dirID);

      void clearStoreUnlocked();

      void cacheAddUnlocked(std::string& dirID, ChunkDirReferencer* dirRefer);
      void cacheRemoveUnlocked(std::string& dirID);
      void cacheRemoveAllUnlocked();
      bool cacheSweepUnlocked(bool isSyncSweep);

      bool mkdirV2ChunkDirPath(int targetFD, const Path* chunkDirPath);

      bool mkdirChunkDirPath(int targetFD, const Path* chunkDirPath, bool hasOrigFeature,
         ChunkDir** outChunkDir);

      std::pair<FhgfsOpsErr, int> openAndChown(const int targetFD, const std::string& path,
         const int openFlags, const SessionQuotaInfo& quota);

      // inlined

      /**
       * Return a unique path element identifier.
       *
       * Note: All callers should use depth=0 for the first path element.
       */
      std::string getUniqueDirID(std::string pathElement, unsigned pathDepth)
      {
         // Use snprintf here directly to make it cheaper?
         return pathElement + "-l" + StringTk::uintToStr(pathDepth);
      }

};

#endif /*CHUNKSTORE_H_*/
