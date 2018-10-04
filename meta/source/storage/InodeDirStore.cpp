#include <common/threading/RWLockGuard.h>
#include <common/threading/UniqueRWLock.h>
#include <program/Program.h>
#include <storage/PosixACL.h>
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

bool InodeDirStore::dirInodeInStoreUnlocked(const std::string& dirID)
{
   DirectoryMapIter iter = this->dirs.find(dirID);
   return iter != this->dirs.end();
}


/**
 * Note: remember to call releaseDir()
 * 
 * @param forceLoad  Force to load the DirInode from disk. Defaults to false.
 * @return NULL if no such dir exists (or if the dir is being moved), but cannot be NULL if
 *    forceLoad is false
 */
DirInode* InodeDirStore::referenceDirInode(const std::string& dirID, bool isBuddyMirrored,
   bool forceLoad)
{
   DirInode* dir = NULL;
   bool wasReferenced = true; /* Only try to add to cache if not in memory yet.
                               * Any attempt to add it to the cache causes a cache sweep, which is
                               * rather expensive.
                               * Note: when set to false we also need a write-lock! */

   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   DirectoryMapIter iter;

   iter = dirs.find(dirID);
   if (iter == dirs.end())
   {
      lock.unlock();
      lock.lock(SafeRWLock_WRITE);
      iter = dirs.find(dirID);
   }

   if(iter == this->dirs.end() )
   { // Not in map yet => try to load it. We must be write-locked here!
      iter = insertDirInodeUnlocked(dirID, isBuddyMirrored, forceLoad);
      wasReferenced = false;
   }

   if(iter != this->dirs.end() )
   { // exists in map
      DirectoryReferencer* dirRefer = iter->second;
      DirInode* dirNonRef = dirRefer->getReferencedObject();

      if(!dirNonRef->getExclusive() ) // check moving
      {
         dir = dirRefer->reference();
         LOG_DBG(GENERAL, SPAM,  "referenceDirInode", dir->getID(), dirRefer->getRefCount());

         if (!wasReferenced)
            cacheAddUnlocked(dirID, dirRefer);
      }

      // no "else".
      // note: we don't need to check unload here, because exclusive means there already is a
      //       reference so we definitely didn't load here
   }

   lock.unlock();

   /* Only try to load the DirInode after giving up the lock. DirInodes are usually referenced
    * without being loaded at all from the kernel client, so we can afford the extra lock if loading
    * the DirInode fails. */
   if (forceLoad && dir && (!dir->loadIfNotLoaded()) )
   { // loading from disk failed, release the dir again
      releaseDir(dirID);
      dir = NULL;
   }

   return dir;
}

/**
 * Release reduce the refcounter of an DirInode here
 */
void InodeDirStore::releaseDir(const std::string& dirID)
{
   RWLockGuard lock(this->rwlock, SafeRWLock_WRITE);
   releaseDirUnlocked(dirID);
}

void InodeDirStore::releaseDirUnlocked(const std::string& dirID)
{
   App* app = Program::getApp();

   DirectoryMapIter iter = this->dirs.find(dirID);
   if(likely(iter != this->dirs.end() ) )
   { // dir exists => decrease refCount
      DirectoryReferencer* dirRefer = iter->second;

      if (likely(dirRefer->getRefCount() ) )
      {
         dirRefer->release();
         DirInode* dirNonRef = dirRefer->getReferencedObject();

         LOG_DBG(GENERAL, SPAM, "releaseDirInode", dirID, dirRefer->getRefCount());

         if(!dirRefer->getRefCount() )
         { // dropped last reference => unload dir

            if (unlikely( dirNonRef->fileStore.getSize() && !app->getSelfTerminate() ) )
            {
               LOG(GENERAL, ERR, "Bug: Refusing to release the directory, "
                     "its fileStore still has references!", dirID);

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
         LOG(GENERAL, ERR, "Bug: Refusing to release dir with a zero refCount", dirID);
         this->dirs.erase(iter);
      }
   }
   else
   {
      LOG(GENERAL, ERR, "Bug: releaseDir requested, but dir not referenced!", dirID);
      LogContext(__func__).logBacktrace();
   }
}

FhgfsOpsErr InodeDirStore::removeDirInode(const std::string& dirID, bool isBuddyMirrored)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   cacheRemoveUnlocked(dirID); /* we should move this after isRemovable()-check as soon as we can
      remove referenced dirs */

   FhgfsOpsErr removableRes = isRemovableUnlocked(dirID, isBuddyMirrored);
   if(removableRes != FhgfsOpsErr_SUCCESS)
      return removableRes;

   bool persistenceOK = DirInode::unlinkStoredInode(dirID, isBuddyMirrored);
   if(!persistenceOK)
      return FhgfsOpsErr_INTERNAL;

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Check whether the dir is removable (i.e. not in use).
 *
 * Note: Caller must make sure that there is no cache reference that would lead to a false in-use
 * result.
 *
 * @param outMirrorNodeID if this returns success and the dir is mirrored, this param will be set to
 * the mirror of this dir, i.e. !=0; (note: mirroring has actually nothing to do with removable
 * check, but we have it here because this method already loads the dir inode, so that we can avoid
 * another inode load in removeDirInodeUnlocked() for mirror checking.)
 */
FhgfsOpsErr InodeDirStore::isRemovableUnlocked(const std::string& dirID, bool isBuddyMirrored)
{
   const char* logContext = "InodeDirStore check if dir is removable";
   DirectoryMapCIter iter = dirs.find(dirID);

   if(iter != dirs.end() )
   { // dir currently loaded, refuse to let it rmdir'ed
      DirectoryReferencer* dirRefer = iter->second;
      DirInode* dir = dirRefer->getReferencedObject();

      if (unlikely(!dirRefer->getRefCount() ) )
      {
         LogContext(logContext).logErr("Bug: unreferenced dir found! EntryID: " + dirID);

         return FhgfsOpsErr_INTERNAL;
      }

      dir->loadIfNotLoaded();

      if(dir->getNumSubEntries() )
         return FhgfsOpsErr_NOTEMPTY;

      if(dir->getExclusive() ) // Someone else is already trying to delete it.
         return FhgfsOpsErr_INUSE;

      // Dir is still referenced (e.g. because someone is just trying to create a file in it.)
      return FhgfsOpsErr_INUSE;
   }
   else
   { // not loaded => static checking
      DirInode dirInode(dirID, isBuddyMirrored);

      if (!dirInode.loadFromFile() )
         return FhgfsOpsErr_PATHNOTEXISTS;

      if(dirInode.getNumSubEntries() )
         return FhgfsOpsErr_NOTEMPTY;
   }

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Note: This does not load any entries, so it will only return the number of already loaded
 * entries. (Only useful for debugging and statistics probably.)
 */
size_t InodeDirStore::getSize()
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   return dirs.size();
}

/**
 * @param outParentNodeID may be NULL
 * @param outParentEntryID may be NULL (if outParentNodeID is NULL)
 */
FhgfsOpsErr InodeDirStore::stat(const std::string& dirID, bool isBuddyMirrored,
   StatData& outStatData, NumNodeID* outParentNodeID, std::string* outParentEntryID)
{
   App* app = Program::getApp();

   FhgfsOpsErr statRes = FhgfsOpsErr_PATHNOTEXISTS;

   NumNodeID expectedOwnerID = isBuddyMirrored
      ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() )
      : app->getLocalNode().getNumID();

   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   DirectoryMapIter iter = dirs.find(dirID);
   if(iter != dirs.end() )
   { // dir loaded
      DirectoryReferencer* dirRefer = iter->second;
      DirInode* dir = dirRefer->getReferencedObject();

      bool loadRes = dir->loadIfNotLoaded(); // might not be loaded at all...
      if (!loadRes)
      {
         return statRes; /* Loading from disk failed, the dir is only referenced, but does not exist
                          * Might be due to our dir-reference optimization or a corruption problem */
      }

      if(expectedOwnerID != dir->getOwnerNodeID() )
      {
         /* check for owner node ID is especially important for root dir
          * NOTE: this check is only performed, if directory is already loaded, because all MDSs
          * need to be able to reference the root directory even if owner is not set (it is not set
          * on first startup). */
         LOG(GENERAL, WARNING, "Owner verification failed.", dirID);
         statRes = FhgfsOpsErr_NOTOWNER;
      }
      else
      {
         statRes = dir->getStatData(outStatData, outParentNodeID, outParentEntryID);
      }

      lock.unlock();
   }
   else
   {
      lock.unlock(); /* Unlock first, no need to hold a lock to just read the file from disk.
                          * If xattr or data are not writte yet we just raced and return
                          * FhgfsOpsErr_PATHNOTEXISTS */

      // read data on disk
      statRes = DirInode::getStatData(dirID, isBuddyMirrored, outStatData, outParentNodeID,
         outParentEntryID);
   }

   return statRes;
}

/**
 * @param validAttribs SETATTR_CHANGE_...-Flags
 */
FhgfsOpsErr InodeDirStore::setAttr(const std::string& dirID, bool isBuddyMirrored, int validAttribs,
   SettableFileAttribs* attribs)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   DirectoryMapIter iter = dirs.find(dirID);
   if(iter == dirs.end() )
   { // not loaded => load, apply, destroy
      DirInode dir(dirID, isBuddyMirrored);

      if(dir.loadFromFile() )
      { // loaded
         bool setRes = dir.setAttrData(validAttribs, attribs);

         return setRes ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_INTERNAL;
      }
   }
   else
   { // dir loaded
      DirectoryReferencer* dirRefer = iter->second;
      DirInode* dir = dirRefer->getReferencedObject();

      if(!dir->getExclusive() )
      {
         bool setRes = dir->setAttrData(validAttribs, attribs);

         return setRes ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_INTERNAL;
      }
   }

   return FhgfsOpsErr_PATHNOTEXISTS;
}

void InodeDirStore::invalidateMirroredDirInodes()
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   for (auto it = dirs.begin(); it != dirs.end(); ++it)
   {
      DirInode& dir = *it->second->getReferencedObject();

      if (dir.getIsBuddyMirrored())
         dir.invalidate();
   }
}

/**
 * Creates and empty DirInode and inserts it into the map.
 *
 * Note: We only need to hold a read-lock here, as we check if inserting an entry into the map
 *       succeeded.
 */
DirectoryMapIter InodeDirStore::insertDirInodeUnlocked(const std::string& dirID,
   bool isBuddyMirrored, bool forceLoad)
{
   std::unique_ptr<DirInode> inode(new (std::nothrow) DirInode(dirID, isBuddyMirrored));
   if (unlikely (!inode) )
      return dirs.end(); // out of memory

   if (forceLoad)
   { // load from disk requested
      if (!inode->loadIfNotLoaded() )
         return dirs.end();
   }

   std::pair<DirectoryMapIter, bool> pairRes =
      this->dirs.insert(DirectoryMapVal(dirID, new DirectoryReferencer(inode.release())));

   return pairRes.first;
}

void InodeDirStore::clearStoreUnlocked()
{
   LOG_DBG(GENERAL, DEBUG, "clearStoreUnlocked", dirs.size());

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
void InodeDirStore::cacheAddUnlocked(const std::string& dirID, DirectoryReferencer* dirRefer)
{
   Config* cfg = Program::getApp()->getConfig();

   unsigned cacheLimit = cfg->getTuneDirMetadataCacheLimit();

   if (unlikely(cacheLimit == 0) )
      return; // cache disabled by user config

   // (we do cache sweeping before insertion to make sure we don't sweep the new entry)
   cacheSweepUnlocked(true);

   if(refCache.insert(DirCacheMapVal(dirID, dirRefer->getReferencedObject()) ).second)
   { // new insert => inc refcount
      dirRefer->reference();

      LOG_DBG(GENERAL, SPAM, "InodeDirStore cache add DirInode.", dirID, dirRefer->getRefCount());
   }
}

void InodeDirStore::cacheRemoveUnlocked(const std::string& dirID)
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
      refCache.erase(iter++);
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

   if(refCache.size() <= cacheLimit)
      return false;


   // pick a random start element (note: the element will be removed in first loop pass below)

   unsigned randStart = randGen.getNextInRange(0, removeSkipNum - 1);
   DirCacheMapIter iter = refCache.begin();

   while(randStart--)
      iter++;

   // walk over all cached elements and remove every n-th element
   DirCacheMapSizeT i = 0; // counts elements
   while (iter != refCache.end() )
   {
      if ( (++i % removeSkipNum) == 0)
      {
         releaseDirUnlocked(iter->first);
         refCache.erase(iter++);
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
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   return cacheSweepUnlocked(false);
}

/**
 * @return current number of cached entries
 */
size_t InodeDirStore::getCacheSize()
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   return refCache.size();
}
