#include <app/log/Logger.h>
#include <common/toolkit/StringTk.h>
#include <common/storage/PathInfo.h>
#include <filesystem/FhgfsOpsHelper.h>
#include "FhgfsOpsSuper.h"
#include "FhgfsInode.h"


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
   fhgfsInode->version = 0;
   atomic_set(&fhgfsInode->modified, 0);
   atomic_set(&fhgfsInode->coRWInProg, 0);
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
   uint64_t* outVersion)
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
         retVal = FhgfsOpsRemoting_getFileVersion(app, &this->entryInfo, outVersion);
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
         retVal = FhgfsOpsRemoting_getFileVersion(app, &this->entryInfo, outVersion);
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

         FhgfsInode_entryInfoReadLock(this); // LOCK EntryInfo

         if ((openFlags & OPENFILE_ACCESS_READ) && (app->cfg->eventLogMask & EventLogMask_READ))
         {
            FileEvent_init(&event, FileEventType_READ, dentry);
            eventSent = &event;
         }
         else
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
         this->pattern = ioInfo.pattern;

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

bool FhgfsInode_isCacheValid(FhgfsInode* this, umode_t i_mode, Config* cfg)
{
   unsigned cacheValidityMS;
   unsigned cacheElapsedMS;
   bool cacheValid;


   if(S_ISDIR(i_mode) )
      cacheValidityMS = Config_getTuneDirSubentryCacheValidityMS(cfg); // defaults to 1s
   else
      cacheValidityMS = Config_getTuneFileSubentryCacheValidityMS(cfg); // defaults to 0

   if(!cacheValidityMS) // caching disabled
      cacheValid = false;
   else
   {
      cacheElapsedMS = Time_elapsedMS(&this->dataCacheTime);
      cacheValid = cacheValidityMS > cacheElapsedMS;
   }

   return cacheValid;
}
