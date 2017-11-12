#include <common/threading/SafeMutexLock.h>
#include <program/Program.h>
#include "DirInode.h"
#include "InodeDirStore.h"


#define DIRSTORE_REFCACHE_REMOVE_SKIP_SYNC     (4) /* n-th elements to be removed on sync sweep */
#define DIRSTORE_REFCACHE_REMOVE_SKIP_ASYNC    (3) /* n-th elements to be removed on async sweep */

/**
  * not inlined as we need to include <program/Program.h>
  */
InodeDirStore::InodeDirStore()
{
   Config* cfg = Program::getApp()->getConfig();

   this->refCacheSyncLimit = cfg->getTuneDirMetadataCacheLimit();
   this->refCacheAsyncLimit = refCacheSyncLimit - (refCacheSyncLimit/2);
}

/**
 * @param dir belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 */
FhgfsOpsErr InodeDirStore::makeDirInode(DirInode* dir, bool keepInode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr mkRes = makeDirInodeUnlocked(dir, keepInode);

   safeLock.unlock(); // U N L O C K
   
   return mkRes;
}

/**
 * @param dir belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 */
FhgfsOpsErr InodeDirStore::makeDirInodeUnlocked(DirInode* dir, bool keepInode)
{
   FhgfsOpsErr retVal = dir->storePersistentMetaData();

   if (!keepInode)
      delete(dir);

   return retVal;
}

/**
 * Note: remember to call releaseDir()
 * 
 * @param forceLoad  Force to load the DirInode from disk. Defaults to false.
 * @return NULL if no such dir exists (or if the dir is being moved), but cannot be NULL if
 *    forceLoad is false
 */
DirInode* InodeDirStore::referenceDirInode(std::string dirID, bool forceLoad)
{
   DirInode* dir = NULL;
   bool addToCache = false; /* Only try to add to cache if not in memory yet,
                             * Any attempt to add it to the cache causes a cache sweep, which is
                             * rather expensive. */
   
   SafeRWLock safeLock(&this->rwlock, SafeRWLock_WRITE); // L O C K

   DirectoryMapIter iter = this->dirs.find(dirID);
   if(iter == this->dirs.end() )
   { // not in map yet => try to load it

      // we cannot upgrade from read to write, so we need to unlock and lock again
      // the small race in between needs to be handled by InsertDirInodeUnlocked()
      // FIXME Bernd: Test performance impact of unlock + lock
      // safeLock.unlock();
      // safeLock.lock(SafeRWLock_WRITE); // L O C K

      InsertDirInodeUnlocked(dirID, iter, forceLoad); // (will set "iter != end" if loaded)
      addToCache = true;
   }
   
   if(iter != dirs.end() )
   { // exists in map
      DirectoryReferencer* dirRefer = iter->second;
      DirInode* dirNonRef = dirRefer->getReferencedObject();
      
      if(!dirNonRef->getExclusive() ) // check moving
      {
         dir = dirRefer->reference();

         // dir might have been in the cache already, but unloaded
         if (forceLoad && (dir->loadIfNotLoaded() == false) )
         { // loading from disk failed
            dirRefer->release();
            dir = NULL;
            this->dirs.erase(iter);
         }
         else
         if (addToCache)
            cacheAddUnlocked(dirID, dirRefer);
      }
      else
      {
         // note: we don't need to check unload here, because exclusive means there already is a
         //       reference so we definitely didn't load here
      }
   }
   
   safeLock.unlock(); // U N L O C K
   
   return dir;
}

/**
 * Release reduce the refcounter of an DirInode here
 */
void InodeDirStore::releaseDir(std::string dirID)
{
   SafeRWLock safeLock(&this->rwlock, SafeRWLock_WRITE); // L O C K

   releaseDirUnlocked(dirID);

   safeLock.unlock(); // U N L O C K
}

void InodeDirStore::releaseDirUnlocked(std::string dirID)
{
   const char* logContext = "InodeDirStore::releaseDir";

   App* app = Program::getApp();

   DirectoryMapIter iter = this->dirs.find(dirID);
   if(likely(iter != this->dirs.end() ) )
   { // dir exists => decrease refCount
      DirectoryReferencer* dirRefer = iter->second;

      if (likely(dirRefer->getRefCount() ) )
      {
         dirRefer->release();
         DirInode* dirNonRef = dirRefer->getReferencedObject();

         if(!dirRefer->getRefCount() )
         { // dropped last reference => unload dir

            if (unlikely( dirNonRef->fileStore.getSize() && !app->getSelfTerminate() ) )
            {
               std::string logMsg = "Bug: Refusing to release the directory, "
                  "its fileStore still has references!";
               LogContext(logContext).logErr(logMsg + std::string(" dirID: ") + dirID);

               dirRefer->reference();
            }
            else
            { // as expected, fileStore is empty
               delete(dirRefer);
               this->dirs.erase(iter);
            }
         }
      }
      else
      { // attempt to release a Dir without a refCount
         std::string logMsg = std::string("Bug: Refusing to release dir with a zero refCount") +
            std::string("dirID: ") + dirID;
         LogContext(logContext).logErr(logMsg);
         this->dirs.erase(iter);
      }
   }
   else
   {
      LogContext(logContext).logErr("Bug: releaseDir requested, but dir not referenced");
      LogContext(logContext).logBacktrace();
   }
}

FhgfsOpsErr InodeDirStore::removeDirInode(std::string dirID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr delErr = removeDirInodeUnlocked(dirID, NULL);

   safeLock.unlock(); // U N L O C K

   return delErr;
}

/**
 * Check whether the dir is removable (i.e. not in use).
 *
 * Note: Caller must make sure that there is no cache reference that would lead to a false in-use
 * result.
 */
FhgfsOpsErr InodeDirStore::isRemovableUnlocked(std::string dirID)
{
   FhgfsOpsErr delErr = FhgfsOpsErr_SUCCESS;

   DirectoryMapCIter iter = dirs.find(dirID);
   if(iter != dirs.end() )
   { // dir currently loaded
      DirectoryReferencer* dirRefer = iter->second;
      DirInode* dir = dirRefer->getReferencedObject();

      dir->loadIfNotLoaded();

      if(dir->getNumSubEntries() )
         delErr = FhgfsOpsErr_NOTEMPTY;
      else
      if(dirRefer->getRefCount() )
         delErr = FhgfsOpsErr_INUSE;
      else
      if(dir->getExclusive() )
         delErr = FhgfsOpsErr_INUSE;
   }
   else
   { // not loaded => static checking
      DirInode dirInode(dirID);

      if (!dirInode.loadFromFile() )
         return FhgfsOpsErr_PATHNOTEXISTS;

      if(dirInode.getNumSubEntries() )
         delErr = FhgfsOpsErr_NOTEMPTY;
   }

   return delErr;
}


/**
 * Note: This method does not lock the mutex, so it must already be locked when calling this
 * 
 * @param outRemovedDir will be set to the removed dir which must then be deleted by the caller
 * (can be NULL if the caller is not interested in the dir)
 */
FhgfsOpsErr InodeDirStore::removeDirInodeUnlocked(std::string dirID, DirInode** outRemovedDir)
{
   if(outRemovedDir)
      *outRemovedDir = NULL;
   
   cacheRemoveUnlocked(dirID); /* we should move this after isRemovable()-check as soon as we can
      remove referenced dirs */

   FhgfsOpsErr removableRes = isRemovableUnlocked(dirID);
   if(removableRes != FhgfsOpsErr_SUCCESS)
      return removableRes;

   if(outRemovedDir)
   { // instantiate outDir
      *outRemovedDir = DirInode::createFromFile(dirID);
      if(!(*outRemovedDir) )
         return FhgfsOpsErr_INTERNAL;
   }

   bool persistenceOK = DirInode::unlinkStoredInode(dirID);
   if(!persistenceOK)
   {
      if(outRemovedDir)
      {
         delete(*outRemovedDir);
         *outRemovedDir = NULL;
      }

      return FhgfsOpsErr_INTERNAL;
   }

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Note: This does not load any entries, so it will only return the number of already loaded
 * entries. (Only useful for debugging and statistics probably.)
 */
size_t InodeDirStore::getSize()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K
   
   size_t dirsSize = dirs.size();

   safeLock.unlock(); // U N L O C K

   return dirsSize;
}

bool InodeDirStore::exists(std::string dirID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   bool existsRes = existsUnlocked(dirID);

   safeLock.unlock(); // U N L O C K

   return existsRes;
}


bool InodeDirStore::existsUnlocked(std::string dirID)
{
   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), dirID);

   bool existsRes = false;
   struct stat statBuf;

   int statRes = ::stat(metaFilename.c_str(), &statBuf);
   if(!statRes)
      existsRes = true;

   return existsRes;
}

/**
 * @param disableLoading true to check the in-memory map only (and don't try to load the
 * metadata from disk)
 */
FhgfsOpsErr InodeDirStore::stat(std::string dirID, StatData& outStatData)
{
   const char* logContext = "InodeDirStore (stat)";

   FhgfsOpsErr statRes = FhgfsOpsErr_PATHNOTEXISTS;

   uint16_t localNodeID = Program::getApp()->getLocalNode()->getNumID();
   
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K
   
   DirectoryMapIter iter = dirs.find(dirID);
   if(iter != dirs.end() )
   { // dir loaded
      DirectoryReferencer* dirRefer = iter->second;
      DirInode* dir = dirRefer->getReferencedObject();

      bool loadRes = dir->loadIfNotLoaded(); // might not be loaded at all...
      if (!loadRes)
         goto out; /* Loading from disk failed, the dir is only referenced, but does not exist
                    * Might be due to our dir-reference optimization or a corruption problem */

      if(localNodeID != dir->getOwnerNodeID() )
      {
         /* check for owner node ID is especially important for root dir
          * NOTE: this check is only performed, if directory is already loaded, because all MDSs
          * need to be able to reference the root directory even if owner is not set (it is not set
          * on first startup). */
         LogContext(logContext).log(Log_WARNING, "Owner verification failed. EntryID: " + dirID);
         statRes = FhgfsOpsErr_NOTOWNER;
      }
      else
      {
         statRes = dir->getStatData(outStatData);
      }
   }
   else
   {  // read data on disk
      statRes = DirInode::getStatData(dirID, outStatData);
   }

 out:
   safeLock.unlock(); // U N L O C K
   
   return statRes;
}

/**
 * @param validAttribs SETATTR_CHANGE_...-Flags
 */
FhgfsOpsErr InodeDirStore::setAttr(std::string dirID, int validAttribs,
   SettableFileAttribs* attribs)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;
   
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K
   
   DirectoryMapIter iter = dirs.find(dirID);
   if(iter == dirs.end() )
   { // not loaded => load, apply, destroy
      DirInode dir(dirID);

      if(dir.loadFromFile() )
      { // loaded
         bool setRes = dir.setAttrData(validAttribs, attribs);

         retVal = setRes ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_INTERNAL;
      }
   }
   else
   { // dir loaded
      DirectoryReferencer* dirRefer = iter->second;
      DirInode* dir = dirRefer->getReferencedObject();
      
      if(!dir->getExclusive() )
      {
         bool setRes = dir->setAttrData(validAttribs, attribs);

         retVal = setRes ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_INTERNAL;
      }
   }

   safeLock.unlock(); // U N L O C K
   
   return retVal;
}

/**
 * Creates and empty DirInode and inserts it into the map.
 *
 * Note: We only need to hold a read-lock here, as we check if inserting an entry into the map
 *       succeeded.
 *
 * @return newElemIter only valid if true is returned, untouched otherwise
 */
bool InodeDirStore::InsertDirInodeUnlocked(std::string dirID, DirectoryMapIter& newElemIter,
   bool forceLoad)
{
   bool retVal = false;

   DirInode* inode = new DirInode(dirID);
   if (unlikely (!inode) )
      return retVal; // out of memory

   if (forceLoad)
   { // load from disk requested
      if (!inode->loadIfNotLoaded() )
      {
         delete inode;
         return false;
      }
   }

   std::pair<DirectoryMapIter, bool> pairRes =
      this->dirs.insert(DirectoryMapVal(dirID, new DirectoryReferencer(inode) ) );

   if (pairRes.second == false)
   {
      // element already exists in the map, we raced with another thread
      delete inode;

      newElemIter = this->dirs.find(dirID);
      if (likely (newElemIter != this->dirs.end() ) )
         retVal = true;
   }
   else
   {
      newElemIter  = pairRes.first;
      retVal = true;
   }

   return retVal;
}

void InodeDirStore::clearStoreUnlocked()
{
   LOG_DEBUG("DirectoryStore::clearStoreUnlocked", Log_DEBUG,
      std::string("# of loaded entries to be cleared: ") + StringTk::intToStr(dirs.size() ) );

   cacheRemoveAllUnlocked();

   for(DirectoryMapIter iter = dirs.begin(); iter != dirs.end(); iter++)
   {
      DirectoryReferencer* dirRef = iter->second;

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
void InodeDirStore::cacheAddUnlocked(std::string& dirID, DirectoryReferencer* dirRefer)
{
   // (we do cache sweeping before insertion to make sure we don't sweep the new entry)
   cacheSweepUnlocked(true);

   if(refCache.insert(DirCacheMapVal(dirID, dirRefer) ).second)
   { // new insert => inc refcount
      dirRefer->reference();
   }

}

void InodeDirStore::cacheRemoveUnlocked(std::string& dirID)
{
   DirCacheMapIter iter = refCache.find(dirID);
   if(iter == refCache.end() )
      return;

   releaseDirUnlocked(dirID);

   refCache.erase(iter);
}

void InodeDirStore::cacheRemoveAllUnlocked()
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
bool InodeDirStore::cacheSweepUnlocked(bool isSyncSweep)
{
   // sweeping means we remove every n-th element from the cache, starting with a random element
      // in the range 0 to n
   size_t cacheLimit;
   size_t removeSkipNum;


   // check type of sweep and set removal parameters accordingly

   if(isSyncSweep)
   { // sync sweep settings
      cacheLimit = refCacheSyncLimit;
      removeSkipNum = DIRSTORE_REFCACHE_REMOVE_SKIP_SYNC;
   }
   else
   { // async sweep settings
      cacheLimit = refCacheAsyncLimit;
      removeSkipNum = DIRSTORE_REFCACHE_REMOVE_SKIP_ASYNC;
   }

   if(refCache.size() < cacheLimit)
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
bool InodeDirStore::cacheSweepAsync()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool retVal = cacheSweepUnlocked(false);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * @return current number of cached entries
 */
size_t InodeDirStore::getCacheSize()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   size_t dirsSize = refCache.size();

   safeLock.unlock(); // U N L O C K

   return dirsSize;
}
