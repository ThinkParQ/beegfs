#include <app/log/Logger.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/toolkit/StringTk.h>
#include <common/storage/PathInfo.h>
#include <filesystem/FhgfsOpsHelper.h>
#include <filesystem/FhgfsOpsInode.h>
#include <components/InvalReader.h>
#include "FhgfsOpsSuper.h"
#include "FhgfsInode.h"
#include "filesystem/BeeGFSNFS4Acl.h"
#include <linux/rcupdate.h>


static NumNodeID __FhgfsInode_getSessionMetaID(FhgfsInode* this)
{
   App* app = FhgfsOps_getApp(this->vfs_inode.i_sb);
   NumNodeID id = {0};

   if (!this->entryInfo.owner.isGroup)
      return this->entryInfo.owner.node;

   if (app)
   {
      MirrorBuddyGroupMapper* mapper = App_getMetaBuddyGroupMapper(app);
      if (mapper)
         id.value = MirrorBuddyGroupMapper_getPrimaryTargetID(mapper,
            this->entryInfo.owner.group);
   }

   return id;
}

/**
 * Called once for initialization of new inodes (and not again if they are recycled) when they
 * are pre-alloced by the mem cache.
 *
 * Note: Initializes only the FhGFS part of the inode, not the general VFS part.
 */
void FhgfsInode_initOnce(FhgfsInode* fhgfsInode)
{
   RWLock_init(&fhgfsInode->entryInfoLock);
   Mutex_init(&fhgfsInode->fileHandlesMutex);
   Mutex_init(&fhgfsInode->rangeLockPIDsMutex);
   AtomicInt_init(&fhgfsInode->numRangeLockPIDs, 0);
   IntMap_init(&fhgfsInode->rangeLockPIDs);
   Mutex_init(&fhgfsInode->appendMutex);
   AtomicInt_init(&fhgfsInode->appendFDsOpen, 0);
   RWLock_init(&fhgfsInode->fileCacheLock);
   FhgfsInode_setNumDirtyPages(fhgfsInode, 0);

   fhgfsInode->nfs4_acl_cache = NULL;
}

/**
 * Called each time an inode is alloced from the mem cache.
 *
 * Note: Initializes only the FhGFS part of the inode, not the general VFS part.
 */
void FhgfsInode_allocInit(FhgfsInode* fhgfsInode)
{
   /* note: keep in mind that we don't only do first-time init here, but also reset everything that
      might have been left over after re-cycling */

   int i;

   memset(&fhgfsInode->entryInfo, 0, sizeof(fhgfsInode->entryInfo) );

   fhgfsInode->parentNodeID = (NumNodeID){0};

   memset(&fhgfsInode->pathInfo, 0, sizeof(fhgfsInode->pathInfo) );

   Time_init(&fhgfsInode->dataCacheTime);

   memset(&fhgfsInode->fileHandles, 0, sizeof(fhgfsInode->fileHandles) );

   for(i=0; i < BEEGFS_INODE_FILEHANDLES_NUM; i++)
   {
      fhgfsInode->fileHandles[i].needsAppendLockCleanup = false;

      AtomicInt_set(&(fhgfsInode->fileHandles[i].maxUsedTargetIndex), -1);

      // not zeroed at this place, referenceHandle() will zero the BitStore
      BitStore_init(&(fhgfsInode->fileHandles[i].firstWriteDone), false);
   }

   fhgfsInode->pattern = NULL;

   AtomicInt_set(&fhgfsInode->numRangeLockPIDs, 0);

   fhgfsInode->fileCacheBuffer.buf = NULL;
   fhgfsInode->fileCacheBuffer.bufType = FileBufferType_NONE;

   FhgfsInode_setNumDirtyPages(fhgfsInode, 0);

   AtomicInt_set(&fhgfsInode->writeBackCounter, 0);
   AtomicInt_set(&fhgfsInode->noRemoteIsizeDecrease, 0);
   AtomicInt64_set(&fhgfsInode->lastWriteBackEndOrIsizeWriteTime, 0);

   fhgfsInode->flags = 0;
   fhgfsInode->fileVersion = 0;
   fhgfsInode->metaVersion = 0;
   atomic_set(&fhgfsInode->modified, 0);
   atomic_set(&fhgfsInode->coRWInProg, 0);
   fhgfsInode->localSessionCnt = 0;

   fhgfsInode->nfs4_acl_cache = NULL;
}

/**
 * Called each time an inode is returned to the mem cache.
 *
 * Note: Destroys only the FhGFS part of the inode, not the general VFS part.
 */
void FhgfsInode_destroyUninit(FhgfsInode* fhgfsInode)
{
   /* note: keep in mind that we shouldn't reset everything here because we don't know whether this
      inode will ever be recycled. but we definitely need to free all alloc'ed stuff here. */
   int i;

   /* Drop NFS4 ACL cache (safe: inode going away) */
   {
      struct BeeGFSNFS4AclCache *old_cache;
      /* Atomically swap cache pointer to NULL. */
      old_cache = xchg((struct BeeGFSNFS4AclCache **)&fhgfsInode->nfs4_acl_cache, NULL);
      if (old_cache)
      {
#ifdef LOG_DEBUG_MESSAGES
         App* app = FhgfsOps_getApp(fhgfsInode->vfs_inode.i_sb);
         Logger* log = App_getLogger(app);
         const char* logContext = __func__;

         LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
            "nfs4acl_cache: destroy ino=%lu drop cache=%p",
            fhgfsInode->vfs_inode.i_ino, old_cache);
#endif
         /* Defer the free to an RCU callback instead of blocking this teardown
          * (which can run in reclaim context) on a full grace period. The cache
          * pointer was already xchg'd to NULL above, so no new readers arrive. */
         call_rcu(&old_cache->rcu, BeeGFSNFS4Acl_freeCacheRcu);
      }
   }

   EntryInfo_uninit(&fhgfsInode->entryInfo);

   PathInfo_uninit(&fhgfsInode->pathInfo);

   SAFE_DESTRUCT_NOSET(fhgfsInode->pattern, StripePattern_virtualDestruct);

   IntMap_clear(&fhgfsInode->rangeLockPIDs);

   for(i=0; i < BEEGFS_INODE_FILEHANDLES_NUM; i++)
   {
      BitStore_uninit(&fhgfsInode->fileHandles[i].firstWriteDone);
   }

}

/**
 * @param openFlags OPENFILE_ACCESS_... flags
 * @param allowRWHandle true if you specified read or write flags and would also accept
 * a rw handle to save the overhead for open/close (only appropriate for writepage etc., use
 * _handleTypeToOpenFlags() in this case when you get the RemoteIOInfo).
 * @param handleType pass this to releaseHandle()
 * @param lookupInfo NULL if this is 'normal' open, non-NULL if the file was already remotely opened
 *    by lookup-intent or atomic_open
 * @param outFileHandleID caller may not free or modify (it's not alloced for the caller!)
 */
FhgfsOpsErr FhgfsInode_referenceHandle(FhgfsInode* this, struct dentry* dentry, int openFlags,
   bool allowRWHandle, LookupIntentInfoOut* lookupInfo, FileHandleType* outHandleType,
   uint32_t* outVersion)
{
   App* app = FhgfsOps_getApp(this->vfs_inode.i_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsInode_referenceHandle";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   FhgfsInodeFileHandle* desiredHandle;
   FhgfsInodeFileHandle* rwHandle = &this->fileHandles[FileHandleType_RW];

   *outHandleType = __FhgfsInode_openFlagsToHandleType(openFlags);
   desiredHandle = &this->fileHandles[*outHandleType];

   if(unlikely(Logger_getLogLevel(log) >= Log_SPAM) )
      FhgfsOpsHelper_logOpMsg(Log_SPAM, app, NULL, &this->vfs_inode, logContext, "handle-type: %d",
         *outHandleType);

   Mutex_lock(&this->fileHandlesMutex); // L O C K

   if(desiredHandle->refCount)
   { // desired handle exists => return it
      retVal = __FhgfsInode_referenceTrunc(this, app, openFlags, dentry);
      if (retVal == FhgfsOpsErr_SUCCESS && outVersion)
      {
         /* Snapshot entryInfo under entryInfoReadLock before the remote call.
          * GetFileVersionMsg stores a bare pointer to entryInfo. A concurrent cross-directory
          * hardlink (or rename) can update entryInfo->fileName via entryInfoWriteLock while
          * we hold only fileHandlesMutex. If fileName changes between the dry-run inside
          * NetMessage_getMsgLength and the actual serialization in NetMessage_serialize, the
          * two passes compute different byte counts. Socket_send_kernel then sends the stale
          * (shorter) length, delivering a truncated message to meta that fails deserialization.
          * A stable copy avoids the race without holding any lock over network I/O. */
         EntryInfo entryInfoCopy;
         FhgfsInode_entryInfoReadLock(this);
         EntryInfo_dup(&this->entryInfo, &entryInfoCopy);
         FhgfsInode_entryInfoReadUnlock(this);
         retVal = FhgfsOpsRemoting_getFileVersion(app, &entryInfoCopy, outVersion);
         EntryInfo_uninit(&entryInfoCopy);
      }
      if(retVal != FhgfsOpsErr_SUCCESS)
         goto err_unlock;

      desiredHandle->refCount++;
      //*outFileHandleID = desiredHandle->fileHandleID;
   }
   else
   if(allowRWHandle && rwHandle->refCount)
   { // rw handle allowed and exists => return it
      retVal = __FhgfsInode_referenceTrunc(this, app, openFlags, dentry);
      if (retVal == FhgfsOpsErr_SUCCESS && outVersion)
      {
         /* Same race fix as above - see comment in desiredHandle branch. */
         EntryInfo entryInfoCopy;
         FhgfsInode_entryInfoReadLock(this);
         EntryInfo_dup(&this->entryInfo, &entryInfoCopy);
         FhgfsInode_entryInfoReadUnlock(this);
         retVal = FhgfsOpsRemoting_getFileVersion(app, &entryInfoCopy, outVersion);
         EntryInfo_uninit(&entryInfoCopy);
      }
      if(retVal != FhgfsOpsErr_SUCCESS)
         goto err_unlock;

      rwHandle->refCount++;
      //*outFileHandleID = rwHandle->fileHandleID;
      *outHandleType = FileHandleType_RW;
   }
   else
   { // no matching handle yet => open file to create a new handle

      RemotingIOInfo ioInfo;
      const EntryInfo* entryInfo;
      PathInfo* pathInfo = FhgfsInode_getPathInfo(this);

      if (lookupInfo)
      {  // file already open by lookup/atomic_open
         entryInfo            = lookupInfo->entryInfoPtr;
         ioInfo.fileHandleID  = lookupInfo->fileHandleID;
         ioInfo.pattern       = lookupInfo->stripePattern;
         PathInfo_update(pathInfo, &lookupInfo->pathInfo);

         retVal = FhgfsOpsErr_SUCCESS;
      }
      else
      {  // file yet opened by lookup or atomic open
         struct FileEvent event = FILEEVENT_EMPTY;
         const struct FileEvent* eventSent = NULL;
         __FhgfsInode_initOpenIOInfo(this, desiredHandle, openFlags, pathInfo, &ioInfo);

         if (FhgfsInode_isStripePatternStale(this)) //check if pattern is stale (due to internal metadata changes)
         //protected by fileHandlesMutex against races here
         {
            ioInfo.pattern = NULL;
         }

         FhgfsInode_entryInfoReadLock(this); // LOCK EntryInfo

         // Prioritize the evaluation of read-write flags to determine the appropriate event type.
         // The order of assessment is critical as only one event will be dispatched for a
         // file's open() operation, contingent upon the supplied read-write flags.

         // The sequence for evaluating read-write flags is as follows:
         // 1. Read and Write
         // 2. Write Only
         // 3. Read Only
         if ((openFlags & OPENFILE_ACCESS_READWRITE) &&
            (app->cfg->eventLogMask & EventLogMask_OPEN_READ_WRITE))
         {
            FileEvent_init(&event, FileEventType_OPEN_READ_WRITE, dentry);
            eventSent = &event;
         }
         else if ((openFlags & OPENFILE_ACCESS_WRITE) &&
            (app->cfg->eventLogMask & EventLogMask_OPEN_WRITE))
         {
            FileEvent_init(&event, FileEventType_OPEN_WRITE, dentry);
            eventSent = &event;
         }
         else if ((openFlags & OPENFILE_ACCESS_READ) &&
            (app->cfg->eventLogMask & EventLogMask_OPEN_READ))
         {
            FileEvent_init(&event, FileEventType_OPEN_READ, dentry);
            eventSent = &event;
         }

         if ((openFlags & OPENFILE_ACCESS_TRUNC) && (app->cfg->eventLogMask & EventLogMask_TRUNC))
         {
            FileEvent_init(&event, FileEventType_TRUNCATE, dentry);
            eventSent = &event;
         }

         entryInfo = FhgfsInode_getEntryInfo(this);

         retVal = FhgfsOpsRemoting_openfile(entryInfo, &ioInfo, outVersion, eventSent);

         FileEvent_uninit(&event);

         FhgfsInode_entryInfoReadUnlock(this); // UNLOCK EntryInfo
      }


      if(retVal == FhgfsOpsErr_SUCCESS)
      {
         unsigned stripeCount;

         desiredHandle->fileHandleID = ioInfo.fileHandleID;
         // Free memory allocated to stale stripe pattern
         if (FhgfsInode_isStripePatternStale(this) && this->pattern != NULL)
         {
            StripePattern_virtualDestruct(this->pattern);
         }
         this->pattern = ioInfo.pattern;

         if (FhgfsInode_isStripePatternStale(this))
         {
            FhgfsInode_clearStripePatternStaleFlag(this); // clear stale pattern flag
         }

         stripeCount = FhgfsInode_getStripeCount(this);
         BitStore_setSize(&desiredHandle->firstWriteDone, stripeCount);
         BitStore_clearBits(&desiredHandle->firstWriteDone);

         desiredHandle->refCount++;
         //*outFileHandleID = ioInfo.fileHandleID;
      }

   }


err_unlock:

   // clean up

   Mutex_unlock(&this->fileHandlesMutex); // U N L O C K

   return retVal;
}

/**
 * Note: even if this results in a (remote) error, the ref count will be decreased and the caller
 * may no longer access the corresponding ressources.
 *
 * @param handleType what you got back from _referenceHandle()
 */
FhgfsOpsErr FhgfsInode_releaseHandle(FhgfsInode* this, FileHandleType handleType,
   struct dentry* dentry)
{
   App* app = FhgfsOps_getApp(this->vfs_inode.i_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsInode_releaseHandle";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   FhgfsInodeFileHandle* handle = &this->fileHandles[handleType];

   if(unlikely(Logger_getLogLevel(log) >= 5) )
      FhgfsOpsHelper_logOp(Log_SPAM, app, NULL, &this->vfs_inode, logContext);

   Mutex_lock(&this->fileHandlesMutex); // L O C K

   if(unlikely(!handle->refCount) )
      BEEGFS_BUG_ON(true, "refCount already 0"); // refcount already zero => bug
   else
   { // positive refCount
      handle->refCount--;

      if(!handle->refCount)
      { // we dropped the last reference => close remote
         int openFlags = FhgfsInode_handleTypeToOpenFlags(handleType);
         RemotingIOInfo ioInfo;
         const EntryInfo* entryInfo;
         PathInfo* pathInfo = NULL; // not required here
         struct FileEvent event = FILEEVENT_EMPTY;
         struct FileEvent* eventSent = NULL;

         __FhgfsInode_initOpenIOInfo(this, handle, openFlags, pathInfo, &ioInfo);

         FhgfsInode_entryInfoReadLock(this); // LOCK EntryInfo

         entryInfo = FhgfsInode_getEntryInfo(this);

         if (handleType != FileHandleType_READ && (app->cfg->eventLogMask & EventLogMask_CLOSE))
         {
            FileEvent_init(&event, FileEventType_CLOSE_WRITE, dentry);
            eventSent = &event;
         }

         // &event is destroyed by callee
         retVal = FhgfsOpsHelper_closefileWithAsyncRetry(entryInfo, &ioInfo, eventSent);

         FhgfsInode_entryInfoReadUnlock(this); // UNLOCK EntryInfo

         LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
            "handleID: %s remoting complete. result: %s", handle->fileHandleID,
            FhgfsOpsErr_toErrString(retVal) );

         // clean up
         kfree(handle->fileHandleID);
         handle->needsAppendLockCleanup = false;
         AtomicInt_set(&handle->maxUsedTargetIndex, -1);
         BitStore_setSize(&handle->firstWriteDone, 0); // free extra mem from very high stripe count
      }
   }

   Mutex_unlock(&this->fileHandlesMutex); // U N L O C K



   return retVal;
}


bool FhgfsInode_hasWriteHandle(FhgfsInode* this)
{
   bool retVal = false;
   FileHandleType handleType;
   FhgfsInodeFileHandle* handle;

   Mutex_lock(&this->fileHandlesMutex); // L O C K

   handleType = FileHandleType_WRITE;
   handle = &this->fileHandles[handleType];
   if (handle->refCount)
   {
      retVal = true;
      goto out;
   }

   handleType = FileHandleType_RW;
   handle = &this->fileHandles[handleType];
   if (handle->refCount)
   {
      retVal = true;
      goto out;
   }

out:
   Mutex_unlock(&this->fileHandlesMutex); // U N L O C K

   return retVal;
}

/**
 * Remove a pid (if it existed) and decrease the numRangeLockPIDs counter.
 *
 * @return true if the pid was found (i.e. was previously added via _addRangeLockPID() ).
 */
bool FhgfsInode_removeRangeLockPID(FhgfsInode* this, int pid)
{
   bool pidExisted;

   Mutex_lock(&this->rangeLockPIDsMutex); // L O C K

   pidExisted = IntMap_erase(&this->rangeLockPIDs, pid);

   if(pidExisted)
      AtomicInt_dec(&this->numRangeLockPIDs);

   Mutex_unlock(&this->rangeLockPIDsMutex); // U N L O C K

   return pidExisted;
}

/**
 * Add a new pid (if it didn't exist already) and increased the numRangeLockPIDs counter.
 */
void FhgfsInode_addRangeLockPID(FhgfsInode* this, int pid)
{
   bool pidInserted;

   Mutex_lock(&this->rangeLockPIDsMutex); // L O C K

   pidInserted = IntMap_insert(&this->rangeLockPIDs, pid, NULL);

   if(pidInserted)
      AtomicInt_inc(&this->numRangeLockPIDs);

   Mutex_unlock(&this->rangeLockPIDsMutex); // U N L O C K
}


/**
 * Converts OPENFILE_ACCESS_... fhgfs flags to FileHandleType
 */
FileHandleType __FhgfsInode_openFlagsToHandleType(int openFlags)
{
   switch(openFlags & OPENFILE_ACCESS_MASK_RW)
   {
      case OPENFILE_ACCESS_WRITE:
      {
         return FileHandleType_WRITE;
      } break;

      case OPENFILE_ACCESS_READWRITE:
      {
         return FileHandleType_RW;
      } break;

      default:
      {
         return FileHandleType_READ;
      } break;
   }
}

/**
 * Truncates the file if OPENFILE_ACCESS_TRUNC has been specified in openFlags.
 */
FhgfsOpsErr __FhgfsInode_referenceTrunc(FhgfsInode* this, App* app, int openFlags,
      struct dentry* dentry)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   if(openFlags & OPENFILE_ACCESS_TRUNC)
   {
      struct FileEvent event = FILEEVENT_EMPTY;
      const struct FileEvent* eventSent = NULL;
      const EntryInfo* entryInfo;

      FhgfsInode_entryInfoReadLock(this); // LOCK EntryInfo

      entryInfo = FhgfsInode_getEntryInfo(this);

      if (app->cfg->eventLogMask & EventLogMask_TRUNC)
      {
         FileEvent_init(&event, FileEventType_TRUNCATE, dentry);
         eventSent = &event;
      }

      retVal = FhgfsOpsRemoting_truncfile(app, entryInfo, 0, eventSent);

      FileEvent_uninit(&event);

      FhgfsInode_entryInfoReadUnlock(this); // UNLOCK EntryInfo
   }

   return retVal;
}

/**
 * Init minimal ioInfo for mds open/close (referenceHandle/releaseHandle), so without the values
 * that are e.g. only required for read/write etc.
 */
void __FhgfsInode_initOpenIOInfo(FhgfsInode* this, FhgfsInodeFileHandle* fileHandle,
   unsigned accessFlags, PathInfo* pathInfo, RemotingIOInfo* outIOInfo)
{
   outIOInfo->app = FhgfsOps_getApp(this->vfs_inode.i_sb);

   outIOInfo->fileHandleID = fileHandle->fileHandleID;
   outIOInfo->pattern = this->pattern;

   outIOInfo->pathInfo = pathInfo;
   outIOInfo->accessFlags = accessFlags;

   outIOInfo->needsAppendLockCleanup = &fileHandle->needsAppendLockCleanup;
   outIOInfo->maxUsedTargetIndex = &fileHandle->maxUsedTargetIndex;
   outIOInfo->firstWriteDone = NULL;

   outIOInfo->userID  = i_uid_read(&this->vfs_inode);
   outIOInfo->groupID = i_gid_read(&this->vfs_inode);
#ifdef BEEGFS_NVFS
   outIOInfo->nvfs = false;
#endif

#if defined(KERNEL_HAS_INODE_I_WRITE_HINT)
   outIOInfo->writeHint = this->vfs_inode.i_write_hint;
#else
   outIOInfo->writeHint = RW_HINT_INVALID;
#endif
}

/**
 * Retrieve IO information for a file which was previously referenced (i.e. opened).
 *
 * Note: This may only be used by callers that have aqcuired a reference, either explicitly by
 * calling _referenceHandle() or implicitly via open (=> struct file::FsFileInfo).
 *
 * @param outIOInfo the IO info will be stored in this out-arg; no buffers will be allocated, so
 * there is no need to cleanup anything afterwards; but be sure to only use this while your
 * reference is valid.
 */
void FhgfsInode_getRefIOInfo(FhgfsInode* this, FileHandleType handleType,
   unsigned accessFlags, RemotingIOInfo* outIOInfo)
{
   FhgfsInodeFileHandle* fileHandle = &this->fileHandles[handleType];

   __FhgfsInode_initOpenIOInfo(this, fileHandle, accessFlags, &this->pathInfo, outIOInfo);

   // remaining values, which are not assigned by initOpenIOInfo()...

   outIOInfo->firstWriteDone = &fileHandle->firstWriteDone;
}

/**
 * Acquire the internal file cache lock.
 *
 * Note: remember to call the corresponding unlock method!!
 */
void FhgfsInode_fileCacheSharedLock(FhgfsInode* this)
{
   RWLock_readLock(&this->fileCacheLock);
}

void FhgfsInode_fileCacheSharedUnlock(FhgfsInode* this)
{
   RWLock_readUnlock(&this->fileCacheLock);
}

/**
 * Acquire the internal file cache lock.
 *
 * Note: remember to call the corresponding unlock method!!
 */
void FhgfsInode_fileCacheExclusiveLock(FhgfsInode* this)
{
   RWLock_writeLock(&this->fileCacheLock);
}

/**
 * Acquire the internal file cache lock if it is available immediately.
 *
 * Note: remember to call the corresponding unlock method (if the lock has been acquired)!!
 *
 * @return 1 if lock acquired, 0 if contention
 */
int FhgfsInode_fileCacheExclusiveTryLock(FhgfsInode* this)
{
   return RWLock_writeTryLock(&this->fileCacheLock);
}

void FhgfsInode_fileCacheExclusiveUnlock(FhgfsInode* this)
{
   RWLock_writeUnlock(&this->fileCacheLock);
}

/**
 * Get the current attached file cache entry.
 * Caller must hold the fileCache lock to work with this cache entry!
 */
struct CacheBuffer* Fhgfsinode_getFileCacheBuffer(FhgfsInode* this)
{
   return &this->fileCacheBuffer;
}

/**
 * Generate IDs suitable for i_ino.
 *
 * @param entryID zero-terminated string
 * @param entryIDLen entryID length without terminating zero
 */
uint64_t FhgfsInode_generateInodeID(struct super_block* sb, const char* entryID, int entryIDLen)
{
   App* app = FhgfsOps_getApp(sb);
   Config* cfg = App_getConfig(app);
   InodeIDStyle inodeIDStyle = Config_getSysInodeIDStyleNum(cfg);
   size_t hashBits; // 32 or 64 bit

   if(sizeof(ino_t) >= sizeof(uint64_t) )
      hashBits = 64;
   else
      hashBits = 32;

   /* note: iunique() or something else, which doesn't generate reproducible results isn't
      allowed here, because we need a reproducible i_ino/hashval for
      iget5_locked/__FhgfsOps_compareInodeID. */


   switch(inodeIDStyle)
   {
      case INODEIDSTYLE_Hash64HSieh:
      {
         uint64_t hashRes = HashTk_hash(HASHTK_HSIEHHASH32, hashBits, entryID, entryIDLen);

         // add terminating zero to hash buffer if resultig ID is too low
         if(unlikely(hashRes <= BEEGFS_INODE_MAXRESERVED_INO) )
            hashRes = HashTk_hash(HASHTK_HSIEHHASH32, hashBits, entryID, entryIDLen+1);

         return hashRes;

      } break;

      case INODEIDSTYLE_Hash64MD4:
      {
         uint64_t hashRes = HashTk_hash(HASHTK_HALFMD4, hashBits, entryID, entryIDLen);

         // add terminating zero to hash buffer if resultig ID is too low
         if(unlikely(hashRes <= BEEGFS_INODE_MAXRESERVED_INO) )
            hashRes = HashTk_hash(HASHTK_HALFMD4, hashBits, entryID, entryIDLen+1);

         return hashRes;
      } break;

      case INODEIDSTYLE_Hash32MD4:
      { // generate 32bit hash of fhgfs entryID string

         uint64_t hashRes = HashTk_hash32(HASHTK_HALFMD4, entryID, entryIDLen);

         // add terminating zero to hash buffer if resultig ID is too low
         if(unlikely(hashRes <= BEEGFS_INODE_MAXRESERVED_INO) )
            hashRes = HashTk_hash32(HASHTK_HALFMD4, entryID, entryIDLen+1);

         return hashRes;

      } // fall through to hsieh32bit hash on 32bit systems...

      default:
      { // generate 32bit hash of fhgfs entryID string
         uint32_t hashRes = HashTk_hash32(HASHTK_HSIEHHASH32, entryID, entryIDLen);

         // add terminating zero to hash buffer if resultig ID is too low
         if(unlikely(hashRes <= BEEGFS_INODE_MAXRESERVED_INO) )
            hashRes = HashTk_hash32(HASHTK_HSIEHHASH32, entryID, entryIDLen+1);

         return hashRes;
      }

   }
}

struct inode* FhgfsInode_GetInodeFromEntryID(struct super_block *sb, char *entryID)
{
   ino_t inodeHash;
   FhgfsInodeComparisonInfo comparisonInfo;

   size_t entryIDLen = strlen(entryID);

   if (strncmp(entryID, META_ROOTDIR_ID_STR, entryIDLen) == 0)
      inodeHash = BEEGFS_INODE_ROOT_INO;
   else
      inodeHash = FhgfsInode_generateInodeID(sb, entryID, entryIDLen);

   comparisonInfo.inodeHash = inodeHash;
   comparisonInfo.entryID = entryID;

   return ilookup5(sb, inodeHash, __FhgfsOps_compareInodeID,
      &comparisonInfo);
}

//caller must already hold entryInfoLock
void FhgfsInode_setCacheValidUnlocked(FhgfsInode* this)
{
   FhgfsInode_setLocalSessionCnt(this);
   FhgfsInode_setValid(this);
}


void FhgfsInode_setCacheValid(FhgfsInode* this)
{
   App* app = FhgfsOps_getApp(this->vfs_inode.i_sb);

   if (!app->invalReader || App_getInvalWatchFallback(app) )
   {
      //avoid taking entryInfoLock if invalReader feature is off
      FhgfsInode_setValid(this);
      return;
   }

   FhgfsInode_entryInfoWriteLock(this);
   FhgfsInode_setCacheValidUnlocked(this);
   FhgfsInode_entryInfoWriteUnlock(this);
}

void FhgfsInode_setLocalSessionCnt(FhgfsInode* this)
{
   App* app = FhgfsOps_getApp(this->vfs_inode.i_sb);
   InvalReader* inval = app ? app->invalReader : NULL;
   NumNodeID sid;
   if (!inval)
      return;
   sid = __FhgfsInode_getSessionMetaID(this);

   this->localSessionCnt = InvalReader_getMetaSessionCnt(inval, sid);
}
unsigned FhgfsInode_getCacheValidityMS(umode_t i_mode, Config* cfg)
{
   if(S_ISDIR(i_mode) )
      return Config_getTuneDirSubentryCacheValidityMS(cfg); // defaults to 1s

   return Config_getTuneFileSubentryCacheValidityMS(cfg); // defaults to 0
}
bool FhgfsInode_isCacheValid(FhgfsInode* this, umode_t i_mode, Config* cfg)
{
   unsigned cacheValidityMS;
   unsigned cacheElapsedMS;
   bool cacheValid;
   App* app = FhgfsOps_getApp(this->vfs_inode.i_sb);
   if (Config_getSysRemoteInvalEnabled(cfg) && !App_getInvalWatchFallback(app))
   {
      FhgfsInode_entryInfoReadLock(this);
      cacheValid = FhgfsInode_isValid(this);
      if (cacheValid &&  app->invalReader)
      {
         NumNodeID sid = __FhgfsInode_getSessionMetaID(this);
         int32_t currSessionCnt = InvalReader_getMetaSessionCnt(app->invalReader, sid);
         //check if sessionCnt is odd (valid) and if it changed
         cacheValid = ((currSessionCnt & 1) == 1) && ((this->localSessionCnt & 1) == 1) && (currSessionCnt == this->localSessionCnt);
      }
      FhgfsInode_entryInfoReadUnlock(this);

      if (cacheValid)
         atomic64_inc(&app->inodeCacheHits);
      else
         atomic64_inc(&app->inodeCacheMisses);

      #ifdef BEEGFS_DEBUG
      if (!cacheValid)
      {
         LOG_DEBUG_FORMATTED(app->logger, Log_DEBUG, __func__, "Found invalidated inode %s", this->entryInfo.entryID);
      }
      #endif
      return cacheValid;
   }
   // Fallback to default timeout mechanism for cache invalidation.
   cacheValidityMS = FhgfsInode_getCacheValidityMS(i_mode, cfg);


   if(!cacheValidityMS) // caching disabled
      cacheValid = false;
   else
   {
      cacheElapsedMS = Time_elapsedMS(&this->dataCacheTime);
      cacheValid = cacheValidityMS > cacheElapsedMS;
   }

   return cacheValid;
}

void FhgfsInode_setParentInfo(FhgfsInode* this, NumNodeID parentNodeID)
{
   FhgfsInode_entryInfoWriteLock(this);

   FhgfsInode_setParentNodeID(this, parentNodeID);
   FhgfsInode_setLocalSessionCnt(this);

   FhgfsInode_entryInfoWriteUnlock(this);
}
