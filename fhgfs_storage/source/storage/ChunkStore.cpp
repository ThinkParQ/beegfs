#include <common/threading/SafeMutexLock.h>
#include <program/Program.h>
#include <common/storage/quota/ExceededQuotaStore.h>
#include <toolkit/StorageTkEx.h>

#include "ChunkStore.h"


#define CHUNKSTORE_REFCACHE_REMOVE_SKIP_SYNC   (4) /* n-th elements to be removed on sync sweep */
#define CHUNKSTORE_REFCACHE_REMOVE_SKIP_ASYNC  (3) /* n-th elements to be removed on async sweep */

/**
  * not inlined as we need to include <program/Program.h>
  */
ChunkStore::ChunkStore()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   this->refCacheSyncLimit = cfg->getTuneDirCacheLimit();
   this->refCacheAsyncLimit = refCacheSyncLimit - (refCacheSyncLimit/2);
}

bool ChunkStore::dirInStoreUnlocked(std::string dirID)
{
   DirectoryMapIter iter = this->dirs.find(dirID);
   if(iter != this->dirs.end() )
      return true;

   return false;
}


/**
 * Note: remember to call releaseDir()
 *
 * @param forceLoad  Force to load the ChunkDir from disk. Defaults to false.
 * @return NULL if no such dir exists (or if the dir is being moved), but cannot be NULL if
 *    forceLoad is false
 */
ChunkDir* ChunkStore::referenceDir(std::string dirID)
{
   const char* logContext = "DirReferencer referenceChunkDir";
   ChunkDir* dir = NULL;
   bool wasReferenced = true; /* Only try to add to cache if not in memory yet.
                               * Any attempt to add it to the cache causes a cache sweep, which is
                               * rather expensive.
                               * Note: when set to false we also need a write-lock! */

   SafeRWLock safeLock(&this->rwlock, SafeRWLock_READ); // L O C K

   DirectoryMapIter iter;
   int retries = 0; // 0 -> read-locked
   while (retries < RWLOCK_LOCK_UPGRADE_RACY_RETRIES) // one as read-lock and one as write-lock
   {
      iter = this->dirs.find(dirID);
      if (iter == this->dirs.end() && retries == 0)
      {
         safeLock.unlock();
         safeLock.lock(SafeRWLock_WRITE);
      }
      retries++;
   }

   if(iter == this->dirs.end() )
   { // Not in map yet => try to load it. We must be write-locked here!
      InsertChunkDirUnlocked(dirID, iter); // (will set "iter != end" if loaded)
      wasReferenced = false;
   }

   if (likely(iter != dirs.end() ) )
   { // exists in map
      ChunkDirReferencer* dirRefer = iter->second;

      dir = dirRefer->reference();

      // LOG_DEBUG(logContext, Log_SPAM,  std::string("DirID: ") + dir->getID() +
      //   " Refcount: " + StringTk::intToStr(dirRefer->getRefCount() ) );
      IGNORE_UNUSED_VARIABLE(logContext);

      if (wasReferenced == false)
         cacheAddUnlocked(dirID, dirRefer);

   }

   safeLock.unlock(); // U N L O C K

   return dir;
}

/**
 * Release reduce the refcounter of an ChunkDir here
 */
void ChunkStore::releaseDir(std::string dirID)
{
   SafeRWLock safeLock(&this->rwlock, SafeRWLock_WRITE); // L O C K

   releaseDirUnlocked(dirID);

   safeLock.unlock(); // U N L O C K
}

void ChunkStore::releaseDirUnlocked(std::string dirID)
{
   const char* logContext = "DirReferencer releaseChunkDir";

   DirectoryMapIter iter = this->dirs.find(dirID);
   if(likely(iter != this->dirs.end() ) )
   { // dir exists => decrease refCount
      ChunkDirReferencer* dirRefer = iter->second;

      if (likely(dirRefer->getRefCount() ) )
      {
         dirRefer->release();

         // LOG_DEBUG(logContext, Log_SPAM,  std::string("DirID: ") + dirID +
         //   " Refcount: " + StringTk::intToStr(dirRefer->getRefCount() ) );

         if(!dirRefer->getRefCount() )
         {  // dropped last reference => unload dir
            delete(dirRefer);
            this->dirs.erase(iter);
         }
      }
      else
      {  // attempt to release a Dir without a refCount
         std::string logMsg = std::string("Bug: Refusing to release dir with a zero refCount") +
            std::string("dirID: ") + dirID;
         LogContext(logContext).logErr(logMsg);
         this->dirs.erase(iter);
      }
   }
   else
   {
      LogContext(logContext).logErr("Bug: releaseDir requested, but dir not referenced! "
         "DirID: " + dirID);
      LogContext(logContext).logBacktrace();
   }
}


/**
 * Creates and empty ChunkDir and inserts it into the map.
 *
 * Note: We only need to hold a read-lock here, as we check if inserting an entry into the map
 *       succeeded.
 *
 * @return newElemIter only valid if true is returned, untouched otherwise
 */
void ChunkStore::InsertChunkDirUnlocked(std::string dirID, DirectoryMapIter& newElemIter)
{
   ChunkDir* inode = new ChunkDir(dirID);
   if (unlikely (!inode) )
      return;

   std::pair<DirectoryMapIter, bool> pairRes =
      this->dirs.insert(DirectoryMapVal(dirID, new ChunkDirReferencer(inode) ) );

   if (pairRes.second == false)
   {
      // element already exists in the map, we raced with another thread
      delete inode;

      newElemIter = this->dirs.find(dirID);
   }
   else
   {
      newElemIter  = pairRes.first;
   }

}


void ChunkStore::clearStoreUnlocked()
{
   LOG_DEBUG("DirectoryStore::clearStoreUnlocked", Log_DEBUG,
      std::string("# of loaded entries to be cleared: ") + StringTk::intToStr(dirs.size() ) );

   cacheRemoveAllUnlocked();

   for(DirectoryMapIter iter = dirs.begin(); iter != dirs.end(); iter++)
   {
      ChunkDirReferencer* dirRef = iter->second;

      // will also call destructor for dirInode and sub-objects as dirInode->fileStore
      delete(dirRef);
   }

   dirs.clear();
}

/**
 * Note: Make sure to call this only after the new reference has been taken by the caller
 * (otherwise it might happen that the new element is deleted during sweep if it was cached
 * before and appears to be unneeded now).
 */
void ChunkStore::cacheAddUnlocked(std::string& dirID, ChunkDirReferencer* dirRefer)
{
   const char* logContext = "DirReferencer cache add ChunkDir";

   // (we do cache sweeping before insertion to make sure we don't sweep the new entry)
   cacheSweepUnlocked(true);

   if(refCache.insert(DirCacheMapVal(dirID, dirRefer) ).second)
   { // new insert => inc refcount
      dirRefer->reference();

      // LOG_DEBUG(logContext, Log_SPAM,  std::string("DirID: ") + dirID +
      //   " Refcount: " + StringTk::intToStr(dirRefer->getRefCount() ) );
      IGNORE_UNUSED_VARIABLE(logContext);
   }

}

void ChunkStore::cacheRemoveUnlocked(std::string& dirID)
{
   DirCacheMapIter iter = refCache.find(dirID);
   if(iter == refCache.end() )
      return;

   releaseDirUnlocked(dirID);
   refCache.erase(iter);
}

void ChunkStore::cacheRemoveAllUnlocked()
{
   for(DirCacheMapIter iter = refCache.begin(); iter != refCache.end(); /* iter inc inside loop */)
   {
      releaseDirUnlocked(iter->first);

      DirCacheMapIter iterNext(iter);
      iterNext++;

      // cppcheck-suppress erase [special comment to mute false cppcheck alarm]
      refCache.erase(iter);

      iter = iterNext;
   }
}

/**
 * @param isSyncSweep true if this is a synchronous sweep (e.g. we need to free a few elements to
 * allow quick insertion of a new element), false is this is an asynchronous sweep (that might take
 * a bit longer).
 * @return true if a cache flush was triggered, false otherwise
 */
bool ChunkStore::cacheSweepUnlocked(bool isSyncSweep)
{
   // sweeping means we remove every n-th element from the cache, starting with a random element
      // in the range 0 to n
   size_t cacheLimit;
   size_t removeSkipNum;

   // check type of sweep and set removal parameters accordingly

   if(isSyncSweep)
   { // sync sweep settings
      cacheLimit = refCacheSyncLimit;
      removeSkipNum = CHUNKSTORE_REFCACHE_REMOVE_SKIP_SYNC;
   }
   else
   { // async sweep settings
      cacheLimit = refCacheAsyncLimit;
      removeSkipNum = CHUNKSTORE_REFCACHE_REMOVE_SKIP_ASYNC;
   }

   if(refCache.size() <= cacheLimit)
      return false;


   // pick a random start element (note: the element will be removed in first loop pass below)

   unsigned randStart = randGen.getNextInRange(0, removeSkipNum - 1);
   DirCacheMapIter iter = refCache.begin();

   while(randStart--)
      iter++;


   // walk over all cached elements and remove every n-th element

   unsigned i = removeSkipNum-1; /* counts to every n-th element ("remoteSkipNum-1" to remove the
      random start element in the first loop pass) */

   while(iter != refCache.end() )
   {
      i++;

      if(i == removeSkipNum)
      {
         releaseDirUnlocked(iter->first);

         DirCacheMapIter iterNext(iter);
         iterNext++;

         refCache.erase(iter);

         iter = iterNext;

         i = 0;
      }
      else
         iter++;
   }

   return true;
}

/**
 * @return true if a cache flush was triggered, false otherwise
 */
bool ChunkStore::cacheSweepAsync()
{
   const char* logContext = "Async cache sweep";

   //LOG_DEBUG(logContext, Log_SPAM, "Start cache sweep."); // debug in
   IGNORE_UNUSED_VARIABLE(logContext);

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool retVal = cacheSweepUnlocked(false);

   safeLock.unlock(); // U N L O C K

   // LOG_DEBUG(logContext, Log_SPAM, "Stop cache sweep."); // debug in

   return retVal;
}

/**
 * @return current number of cached entries
 */
size_t ChunkStore::getCacheSize()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   size_t dirsSize = refCache.size();

   safeLock.unlock(); // U N L O C K

   return dirsSize;
}


/**
 * Iterate through chunkDirPath and rmdir each path-element beginning with uidXYZ.
 *
 * Note: Only path elements starting with uidXYZ will be removed.
 * Note: chunkDirPath will be modified - all elements except the first one will be removed.
 */
bool ChunkStore::rmdirChunkDirPath(int targetFD, Path* chunkDirPath)
{
   const char* logContext = "ChunkDirStore rmdir chunkdir path";
   bool retVal = true;

   int uidPos = STORAGETK_CHUNKDIR_VEC_UIDPOS;

   int chunkDirPos = chunkDirPath->size() - 1;

   // Iterate over all path elements in reverse order and try to rmdir up to uidXYZ
   while (chunkDirPos >= uidPos)
   {
      std::string chunkDirID = getUniqueDirID(chunkDirPath->back(), chunkDirPos);

      /* Note: We only write-lock the current dir element. mkdir needs to (read) lock
       *       parent + current, until current was created. */
      ChunkDir* chunkDir = referenceDir(chunkDirID);

      if (likely(chunkDir) )
         chunkDir->writeLock();     // LOCK, Note: SafeRWLock does not work due to the if-condition
      else
         LogContext(logContext).logErr("Bug: Failed to reference chunkDir: " + chunkDirID);

      std::string rmDirPath = chunkDirPath->str();

      int rmdirRes = unlinkat(targetFD, rmDirPath.c_str(), AT_REMOVEDIR);

      int rmDirErrno = errno;

      if (likely(chunkDir) )
      {
         chunkDir->unlock();        // UNLOCK
         releaseDir(chunkDirID);
      }

      if (rmdirRes == -1)
      {
         if ( (rmDirErrno != ENOENT)  && (rmDirErrno != ENOTEMPTY) )
         {
            LogContext(logContext).logErr("Unable to rmdir chunk path: " + rmDirPath + ". " +
               "SysErr: " + System::getErrString() );
            retVal = false;
         }

         break;
      }

      *chunkDirPath = chunkDirPath->dirname();
      chunkDirPos = chunkDirPath->size() - 1;
   }

   return retVal;
}


/**
 * Create V2 (deprecated) chunkDirPath (chunks/hash1/hash2/).
 * Note: No locking required.
 */
bool ChunkStore::mkdirV2ChunkDirPath(int targetFD, Path* chunkDirPath)
{
   const char* logContext = "ChunkDirStore mkdir V2 chunkdir path";
   bool retVal = true;
   unsigned pathElemIndex = 0;

   std::string mkdirPath;

   // Iterate and create basic chunks or mirror paths, we don't need any locking here
   while (pathElemIndex != chunkDirPath->size())
   {
      mkdirPath += (*chunkDirPath)[pathElemIndex];

      int mkdirRes = mkdirat(targetFD, mkdirPath.c_str(), STORAGETK_DEFAULTCHUNKDIRMODE);
      if (mkdirRes && errno != EEXIST)
      {
         LogContext(logContext).logErr("Unable to create chunk path: " + mkdirPath + ". " +
            "SysErr: " + System::getErrString() );

         retVal = false;
         break;
      }

      mkdirPath = mkdirPath + '/'; // path must be relative, so only add it here
      pathElemIndex++;
   }

   return retVal;
}

/**
 * Iterate through chunkDirPath and rmdir each path-element beginning with uidXYZ.
 *
 * @param outChunkDir   The last path element, needs to be unlocked and release by the caller,
 *                      once the caller has created a new file in it.
 */
bool ChunkStore::mkdirChunkDirPath(int targetFD, Path* chunkDirPath, bool hasOrigFeature,
   ChunkDir** outChunkDir)
{
   const char* logContext = "ChunkDirStore mkdir chunkdir path";
   bool retVal;
   *outChunkDir = NULL;

   // V2 version for 2012.10 style layout ...

   if (!hasOrigFeature)
      return mkdirV2ChunkDirPath(targetFD, chunkDirPath);


   // V3 version for 2014.01 style layout (chunks/uidXYZ/level1/level2/parentID/) ...


   unsigned uidPos = STORAGETK_CHUNKDIR_VEC_UIDPOS;

   std::string mkdirPath;
   unsigned depth = 0;
   std::string uidStr;

   ChunkDir* chunkDir = NULL;
   ChunkDir* parentChunkDir = NULL;

   // Iterate and create basic paths, we don't need any locking here
   while (depth < uidPos && depth < chunkDirPath->size())
   {
      mkdirPath += (*chunkDirPath)[depth];

      int mkdirRes = mkdirat(targetFD, mkdirPath.c_str(), STORAGETK_DEFAULTCHUNKDIRMODE);
      if (mkdirRes && errno != EEXIST)
      {
         LogContext(logContext).logErr("Unable to create chunk path: " + mkdirPath + ". " +
            "SysErr: " + System::getErrString() );

         retVal = false;
         goto out;
      }

      mkdirPath = mkdirPath + '/'; // path must be relative, so only add it here
      depth++;
   }

   if (depth != chunkDirPath->size())
      (*chunkDirPath)[depth];

   /* Iterate over the remaining path elements (beginning with uidXYZ),
    * lock their IDs and try to create them */
   while (depth < chunkDirPath->size())
   {
      std::string currentElement = (*chunkDirPath)[depth];
      mkdirPath += currentElement;

      std::string chunkDirID = getUniqueDirID(currentElement, depth);

      chunkDir = referenceDir(chunkDirID);
      if (likely(chunkDir) )
         chunkDir->rwlock.readLock();
      else
         LogContext(logContext).logErr("Bug: Failed to reference chunkDir " + mkdirPath + "!");

      int mkdirRes = mkdirat(targetFD, mkdirPath.c_str(), STORAGETK_DEFAULTCHUNKDIRMODE);

      int mkdirErrno = errno;

      if (parentChunkDir)
      {
         /* Once we keep a lock on the current dir and created it we can give up the lock of the
          * parent - a racing rmdir on parent will fail with ENOTEMPTY.
          * If mkdir failed we do not care, as something is wrong anyway.  */
         parentChunkDir->rwlock.unlock();
         releaseDir(parentChunkDir->getID() );
      }

      if (mkdirRes && mkdirErrno != EEXIST)
      {
         LogContext(logContext).logErr("Unable to create chunk path: " + mkdirPath + ". " +
            "SysErr: " + System::getErrString() );

         if (likely(chunkDir) )
         {
            chunkDir->rwlock.unlock();
            releaseDir(chunkDirID);
         }

         retVal = false;
         goto out;
      }

      mkdirPath = mkdirPath + '/'; // path must be relative, so only add it here
      depth++;
      parentChunkDir = chunkDir;
   }

   if (likely(chunkDir) )
   {
      *outChunkDir = chunkDir;
      retVal = true;
   }
   else
      retVal = false;

out:
   return retVal;
}


/**
 * Create the chunkFile given by chunkFilePathStr and if that fails try to create the
 * chunk directories (path) leading holding the chunk file.
 *
 * @param chunkDirPath can be NULL, then it will be constructed from chunkFilePathStr if needed
 */
FhgfsOpsErr ChunkStore::openChunkFile(int targetFD, Path* chunkDirPath,
   std::string& chunkFilePathStr, bool hasOrigFeature, int openFlags, int* outFD,
   SessionQuotaInfo* quotaInfo)
{
   const char* logContext = "ChunkStore create chunkFile";
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   mode_t openMode = STORAGETK_DEFAULTCHUNKFILEMODE;

   int fd;

   unsigned quotaUID = quotaInfo->uid;
   unsigned quotaGID = quotaInfo->gid;
   unsigned processUID;
   unsigned processGID;
   unsigned previousUID;
   unsigned previousGID;

   ExceededQuotaStore* exQuotaStore = Program::getApp()->getExceededQuotaStore();

   // enforce quota only when the client has quota enabled
   if(quotaInfo->useQuota && quotaInfo->enforceQuota)
   {
      // check if exceeded quotas exists, before doing a more expensive and explicit check
      if(exQuotaStore->someQuotaExceeded() )
      {
         // check if size and inode quota is exceeded
         QuotaExceededErrorType exceededError = exQuotaStore->isQuotaExceeded(quotaInfo->uid,
            quotaInfo->gid);

         if(exceededError != QuotaExceededErrorType_NOT_EXCEEDED)
         {
            //check if a new chunk file is required, last check because it is a expensive operation!
            if(!StorageTk::pathExists(chunkFilePathStr.c_str() ) )
            { //report error only when quota is exceeded and a new chunk file is required
               LogContext(logContext).log(Log_NOTICE, QuotaData::QuotaExceededErrorTypeToString(
                  exceededError) + " UID: " + StringTk::uintToStr(quotaInfo->uid) + " ; GID: " +
                  StringTk::uintToStr(quotaInfo->gid) );

               retVal = FhgfsOpsErr_DQUOT;
               goto out;
            }
         }
      }
   }

   if (quotaInfo->useQuota)
      System::setFsIDs(quotaUID, quotaGID, &processUID, &processGID); // S E T _ U I D

   fd = openat(targetFD, chunkFilePathStr.c_str(), openFlags, openMode);
   if (fd == -1)
   { // hash dir didn't exist yet or real error?

      // reset IDs to process ID
      if (quotaInfo->useQuota)
         System::setFsIDs(processUID, processGID, &previousUID, &previousGID); //R E S E T _ U I D

      if(errno == ENOENT)
      {  // hash dir just didn't exist yet => create it and open again
         Path chunkDirPathTmp;
         if (!chunkDirPath)
         {
            chunkDirPathTmp = chunkFilePathStr;
            chunkDirPathTmp = chunkDirPathTmp.dirname();
            chunkDirPath = &chunkDirPathTmp;
         }

         ChunkDir* lastChunkDirElement;

         bool createPathRes = mkdirChunkDirPath(targetFD, chunkDirPath, hasOrigFeature,
            &lastChunkDirElement);
         if(!createPathRes)
         {
            int errCode = errno;

            LogContext(logContext).logErr("Unable to create path for file: "  +
               chunkFilePathStr + ". " + "SysErr: " + System::getErrString(errCode) );

            retVal = FhgfsOpsErrTk::fromSysErr(errCode);
            goto out;
         }

         if (quotaInfo->useQuota)
            System::setFsIDs(quotaUID, quotaGID, &processUID, &processGID); // S E T _ U I D

         // dir created => try file open/create again...
         fd = openat(targetFD, chunkFilePathStr.c_str(), openFlags, openMode);

         // reset IDs to process ID
         if (quotaInfo->useQuota)
            System::setFsIDs(processUID, processGID, &previousUID, &previousGID);//R E S E T _ U I D

         if (lastChunkDirElement) // old V2 files do not get this
         {
            /* Unlock and release the last element once we have created
             * (or at least tried to create) the file. */
            lastChunkDirElement->unlock();
            releaseDir(lastChunkDirElement->getID() );
         }
      }
      else
      if (errno == EACCES)
      {
         retVal = FhgfsOpsErr_NOTOWNER;
         goto out;
      }

      if(unlikely(fd == -1) )
      {  // error
         LogContext(logContext).logErr(
            "Failed to create file: " + chunkFilePathStr + ". " +
            "SysErr: " + System::getErrString() );

         retVal = FhgfsOpsErrTk::fromSysErr(errno);
         goto out;
      }
   }
   else
   { // success
      // reset IDs to process ID
      if (quotaInfo->useQuota)
         System::setFsIDs(processUID, processGID, &previousUID, &previousGID); // R E S E T _ U I D
   }

   // success

   retVal = FhgfsOpsErr_SUCCESS;
   *outFD = fd;

out:
   return retVal;
}


/**
 * V2 chunkDirs (2012.10 style layout: hashDir1/hashDir2) might have wrong permssions, correct that.
 */
bool ChunkStore::chmodV2ChunkDirPath(int targetFD, Path* chunkDirPath, std::string& entryID)
{
   const char* logContext = "ChunkDirStore chmod V2 chunkdir path";
   bool retVal = true;
   size_t pathElemIndex = 0;
   bool didEntryID = false;

   std::string chmodPath;

   // Iterate and create basic chunks or mirror paths, we don't need any locking here
   while ((pathElemIndex != chunkDirPath->size()) || didEntryID)
   {

      if (pathElemIndex != chunkDirPath->size())
      {
         chmodPath += (*chunkDirPath)[pathElemIndex];
      }
      else
      {
         chmodPath += entryID;
         didEntryID = true;
      }

      int chmodRes = ::fchmodat(targetFD, chmodPath.c_str(), STORAGETK_DEFAULTCHUNKDIRMODE, 0);
      if (chmodRes && errno != ENOENT)
      {
         LogContext(logContext).logErr("Unable to change chunk path permissions: " + chmodPath +
            ". " +"SysErr: " + System::getErrString() );

         retVal = false;
         break;
      }

      chmodPath = chmodPath + '/'; // path must be relative, so only add it here

      if (pathElemIndex < chunkDirPath->size())
         pathElemIndex++;
   }

   return retVal;
}

