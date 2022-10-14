#ifndef FHGFSINODE_H_
#define FHGFSINODE_H_

#include <app/App.h>
#include <app/config/Config.h>
#include <common/Common.h>
#include <common/threading/AtomicInt64.h>
#include <common/threading/Mutex.h>
#include <common/threading/RWLock.h>
#include <common/storage/PathInfo.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/Metadata.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/HashTk.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/tree/IntMapIter.h>
#include <filesystem/FhgfsInode.h>
#include <net/filesystem/RemotingIOInfo.h>
#include <toolkit/BitStore.h>
#include <os/OsCompat.h>

#include "FhgfsOpsSuper.h"


#include <linux/fs.h>
#include <linux/vfs.h>



#define BEEGFS_INODE_ROOT_INO                 2 /* traditional root inode number */
#define BEEGFS_INODE_MAXRESERVED_INO          BEEGFS_INODE_ROOT_INO

#define BEEGFS_INODE_FILEHANDLES_NUM          3 /* r, rw, w (see "enum FileHandleType") */

#define BEEGFS_INODE(inodePointer)            ( (FhgfsInode*)inodePointer)
#define BEEGFS_VFSINODE(fhgfsInodePointer)    ( (struct inode*)fhgfsInodePointer)


#define BEEGFS_INODE_FLAG_WRITE_ERROR         1 // there was a write error with this inode
                                               // if set switches to sync writes to faster notify
                                               // applications
#define BEEGFS_INODE_FLAG_PAGED               2 // the inode was written to from page functions

struct StripePattern; // forward declaration

struct FhgfsInodeFileHandle;
typedef struct FhgfsInodeFileHandle FhgfsInodeFileHandle;

enum FileHandleType;
typedef enum FileHandleType FileHandleType;

enum FileBufferType;
typedef enum FileBufferType FileBufferType;

struct CacheBuffer;
typedef struct CacheBuffer CacheBuffer;

struct FhgfsInode;
typedef struct FhgfsInode FhgfsInode;

struct FhgfsIsizeHints;
typedef struct FhgfsIsizeHints FhgfsIsizeHints;


enum FileHandleType // (note: used as array index for FhgfsInode::fileHandles)
   {FileHandleType_READ=0, FileHandleType_RW=1, FileHandleType_WRITE=2};

enum FileBufferType
   {FileBufferType_NONE=0, FileBufferType_READ=1, FileBufferType_WRITE=2};

struct CacheBuffer
{
   char* buf; // the file contents cache buffer
   //size_t bufLen; // length of buf (not needed, because we have bufUsageMaxLen)
   size_t bufUsageLen; // how much of the buffer is already used up
   size_t bufUsageMaxLen; // how much of the buffer may be used (is min(bufLen, chunk_end) )

   loff_t fileOffset; // the file offset where this buffer starts, -1 for append

   enum FileBufferType bufType;
};

/*
 * Used to avoid isize invalid decreases in paged mode due to outdated server side i_size
 */
struct FhgfsIsizeHints
{
   bool ignoreIsize; // caller knows that the i_size needs to be ignored
   uint64_t timeBeforeRemoteStat; // time before doing a remote stat call
};


// kernel inode cache helpers

extern void FhgfsInode_initOnce(FhgfsInode* this);
extern void FhgfsInode_allocInit(FhgfsInode* this);
extern void FhgfsInode_destroyUninit(FhgfsInode* this);


// public extern

extern FhgfsOpsErr FhgfsInode_referenceHandle(FhgfsInode* this, struct dentry* dentry,
   int openFlags, bool allowRWHandle, LookupIntentInfoOut* lookupInfo,
   FileHandleType* outHandleType, uint64_t* outVersion);
extern FhgfsOpsErr FhgfsInode_releaseHandle(FhgfsInode* this, FileHandleType handleType,
   struct dentry* dentry);
extern bool FhgfsInode_hasWriteHandle(FhgfsInode* this);

extern void FhgfsInode_getRefIOInfo(FhgfsInode* this, FileHandleType handleType,
   unsigned accessFlags, RemotingIOInfo* outIOInfo);

extern void FhgfsInode_fileCacheExclusiveLock(FhgfsInode* this);
extern void FhgfsInode_fileCacheExclusiveUnlock(FhgfsInode* this);
extern int FhgfsInode_fileCacheExclusiveTryLock(FhgfsInode* this);
extern void FhgfsInode_fileCacheSharedLock(FhgfsInode* this);
extern void FhgfsInode_fileCacheSharedUnlock(FhgfsInode* this);

extern struct CacheBuffer* Fhgfsinode_getFileCacheBuffer(FhgfsInode* this);

extern bool FhgfsInode_removeRangeLockPID(FhgfsInode* this, int pid);
extern void FhgfsInode_addRangeLockPID(FhgfsInode* this, int pid);

extern uint64_t FhgfsInode_generateInodeID(struct super_block* sb, const char* entryID,
   int entryIDLen);

extern bool FhgfsInode_isCacheValid(FhgfsInode* this, umode_t i_mode, Config* cfg);


// private extern

extern FileHandleType __FhgfsInode_openFlagsToHandleType(int openFlags);
extern FhgfsOpsErr __FhgfsInode_referenceTrunc(FhgfsInode* this, App* app, int openFlag,
   struct dentry* dentry);
extern void __FhgfsInode_initOpenIOInfo(FhgfsInode* this, FhgfsInodeFileHandle* fileHandle,
   unsigned accessFlags, PathInfo* pathInfo, RemotingIOInfo* outIOInfo);


// getters & setters

static inline const EntryInfo* FhgfsInode_getEntryInfo(FhgfsInode* this);
static inline PathInfo* FhgfsInode_getPathInfo(FhgfsInode* this);

static inline DirEntryType FhgfsInode_getDirEntryType(FhgfsInode* this);

static inline int FhgfsInode_getNumRangeLockPIDs(FhgfsInode* this);
static inline bool FhgfsInode_getIsFileOpen(FhgfsInode* this);
static inline bool FhgfsInode_getIsFileOpenByMultipleReaders(FhgfsInode* this);

static inline void FhgfsInode_setLastWriteBackOrIsizeWriteTime(FhgfsInode* this);
static inline uint64_t FhgfsInode_getLastWriteBackOrIsizeWriteTime(FhgfsInode* this);
static inline int FhgfsInode_getWriteBackCounter(FhgfsInode* this);

static inline void FhgfsInode_setWritePageError(FhgfsInode* this);
static inline int FhgfsInode_getHasWritePageError(FhgfsInode* this);



// inliners

static inline void Fhgfsinode_appendLock(FhgfsInode* this);
static inline void Fhgfsinode_appendUnlock(FhgfsInode* this);

static inline void _FhgfsInode_initRootEntryInfo(FhgfsInode* this);

static inline void FhgfsInode_entryInfoReadLock(FhgfsInode* this);
static inline void FhgfsInode_entryInfoReadUnlock(FhgfsInode* this);
static inline void FhgfsInode_entryInfoWriteLock(FhgfsInode* this);
static inline void FhgfsInode_entryInfoWriteUnlock(FhgfsInode* this);

static inline bool FhgfsInode_compareEntryID(FhgfsInode* this, const char* otherEntryID);
static inline void FhgfsInode_updateEntryInfoOnRenameUnlocked(FhgfsInode* this,
   const EntryInfo* newParentInfo, const char* newEntryName);
static inline void FhgfsInode_updateEntryInfoUnlocked(FhgfsInode* this,
   const EntryInfo* newEntryInfo);

static inline int FhgfsInode_getStripeCount(FhgfsInode* this);
static inline StripePattern* FhgfsInode_getStripePattern(FhgfsInode* this);

static inline int FhgfsInode_handleTypeToOpenFlags(FileHandleType handleType);

static inline void FhgfsInode_invalidateCache(FhgfsInode* this);

static inline void FhgfsInode_incNumDirtyPages(FhgfsInode* this);
static inline void FhgfsInode_decNumDirtyPages(FhgfsInode* this);
static inline uint64_t FhgfsInode_getNumDirtyPages(FhgfsInode* this);
static inline void FhgfsInode_setNumDirtyPages(FhgfsInode* this, uint64_t value);
static inline bool FhgfsInode_getHasDirtyPages(FhgfsInode* this);

static inline void FhgfsInode_setParentNodeID(FhgfsInode* this, NumNodeID parentNodeID);
static inline NumNodeID FhgfsInode_getParentNodeID(FhgfsInode* this);

static inline void FhgfsInode_incWriteBackCounter(FhgfsInode* this);
static inline void FhgfsInode_decWriteBackCounter(FhgfsInode* this);

static inline void FhgfsInode_initIsizeHints(FhgfsInode* this, FhgfsIsizeHints* outISizeHints);

static inline void FhgfsInode_setNoIsizeDecrease(FhgfsInode* this);
static inline void FhgfsInode_unsetNoIsizeDecrease(FhgfsInode* this);
static inline int FhgfsInode_getNoIsizeDecrease(FhgfsInode* this);


static inline void FhgfsInode_setPageWriteFlag(FhgfsInode* this);
static inline void FhgfsInode_clearWritePageError(FhgfsInode* this);
static inline int FhgfsInode_getHasPageWriteFlag(FhgfsInode* this);

/**
 * Represents an open file handle on the mds.
 */
struct FhgfsInodeFileHandle
{
   const char* fileHandleID; // set when this handle is used for a remote file open
   unsigned refCount; // number of references to this handle

   bool needsAppendLockCleanup; // true if append lock release failed

   AtomicInt maxUsedTargetIndex; /* zero-based target index in stripe pattern (-1 means "none").
         this is the highest target index that was read/written, so higher targets don't need
         fsync() or close(). */

   BitStore firstWriteDone; /* one bit per storage target in stripe pattern; bit is set when we send
                               data to this target. */
};

/**
 * Represents a linux file or directory inode with fhgfs extensions.
 */
struct FhgfsInode
{
   struct inode vfs_inode; // linux vfs inode

   EntryInfo entryInfo;
   RWLock entryInfoLock;

   NumNodeID parentNodeID; /* nodeID of the parent directory, should be moved to EntryInfo
                            * in the future. So far only used for NFS exports and only set for
                            * DirInodes in FhgfsOpsExport to reconnect dentries. */

   PathInfo pathInfo;


   Time dataCacheTime; /* last time we updated file contents (monotonic clock) or dir attribs;
                          protected by i_lock */

   Mutex fileHandlesMutex;
   FhgfsInodeFileHandle fileHandles[BEEGFS_INODE_FILEHANDLES_NUM]; // use FileHandleType as index
   struct StripePattern* pattern; // initialized when file is opened; multiple readers allowed

   Mutex rangeLockPIDsMutex;
   AtomicInt numRangeLockPIDs; // modified only with mutex held, but readable without it
   IntMap rangeLockPIDs; /* all PIDs (TGIDs) that used range locking and have this file open
                            (values are dummies) */

   Mutex appendMutex; // for atomic writes with append-flag
   AtomicInt appendFDsOpen;

   struct CacheBuffer fileCacheBuffer; // current buffer cache entry (protected by fileCacheLock)
   RWLock fileCacheLock; /* for sync'ed access to cacheEntry and page cache (required to ensure
      order, e.g. when a new r/w-call comes in while we're about to disable caching) */

   AtomicInt64 numPageCacheDirtyPages; // number of written (=> dirty) bytes since last flush

   /* writeBackCounter and lastWriteBackEndOrIsizeWriteTime are hints for refreshInode to
    * ingore the servers inode size */
   AtomicInt   writeBackCounter;  // number of writeBack threads sending pages to the server
   AtomicInt64 lastWriteBackEndOrIsizeWriteTime;
   AtomicInt noRemoteIsizeDecrease; // if set remote attributes won't decrease the i_size

   int flags; // protected by inode->i_lock
   uint64_t version; // protected by entryInfoLock (which would subsume a more granular lock)
   atomic_t modified; // tracks whether the inode data has been written since its last full flush
   atomic_t coRWInProg; //Coherent read/write in progress.
};


int FhgfsInode_getNumRangeLockPIDs(FhgfsInode* this)
{
   return AtomicInt_read(&this->numRangeLockPIDs);
}

/**
 * Test whether a given file inode is currently open.
 *
 * @return true if the file is currently open.
 */
bool FhgfsInode_getIsFileOpen(FhgfsInode* this)
{
   bool retVal = false;
   int i;

   Mutex_lock(&this->fileHandlesMutex); // L O C K

   for(i=0; i < BEEGFS_INODE_FILEHANDLES_NUM; i++)
   {
      if(this->fileHandles[i].refCount)
      { // we found an open file handle
         retVal = true;
         break;
      }
   }

   Mutex_unlock(&this->fileHandlesMutex); // U N L O C K

   return retVal;
}

/**
 * Test whether a given file inode is currently open for reading by multiple readers.
 *
 * Note: This is specifically intended to give a hint for read-ahead disabling (because the current
 * buffered mode logic could do read-ahead trashing with multiple concurrent readers on the same
 * file). Since this is just a hint, we don't care about mutex locking here.
 *
 * @return true if the file is currently open by more than one reader (including read+write).
 */
bool FhgfsInode_getIsFileOpenByMultipleReaders(FhgfsInode* this)
{
   unsigned numReaders = this->fileHandles[FileHandleType_READ].refCount +
      this->fileHandles[FileHandleType_RW].refCount;

   return (numReaders > 1);
}

/**
 * Acquire the internal append mutex
 *
 * Note: remember to call _appendUnlock()!!
 */
void Fhgfsinode_appendLock(FhgfsInode* this)
{
   Mutex_lock(&this->appendMutex);
}

void Fhgfsinode_appendUnlock(FhgfsInode* this)
{
   Mutex_unlock(&this->appendMutex);
}


/**
 * Get the EntryInfo of this inode
 *
 * Note: you will probably need to call _entryInfoReadLock() before using this (and don't forget to
 * unlock afterwards).
 */
const EntryInfo* FhgfsInode_getEntryInfo(FhgfsInode* this)
{
      return &this->entryInfo;
}

PathInfo* FhgfsInode_getPathInfo(FhgfsInode* this)
{
   return &this->pathInfo;
}


/**
 * Initialize the root inode. We can not do that at mount time as we allow mounts without a
 * connection to the management server.
 */
void _FhgfsInode_initRootEntryInfo(FhgfsInode* this)
{
   struct inode* vfs_inode = &this->vfs_inode;
   struct super_block* sb  = vfs_inode->i_sb;
   bool hasRootInfo = FhgfsOps_getHasRootEntryInfo(sb);

   // unlikely as it will happen only once
   if (unlikely(!hasRootInfo) && (vfs_inode->i_ino == BEEGFS_INODE_ROOT_INO) )
   {
      /* root inode, EntryInfo not yet set during mount, as the meta server might be unknown.
       * Set it now. */

      App* app = FhgfsOps_getApp(sb);

      RWLock_writeLock(&this->entryInfoLock); // Write-LOCK

      hasRootInfo = FhgfsOps_getHasRootEntryInfo(sb); // verify under a lock
      if (unlikely(hasRootInfo) )
      {
         // another thread updated it already, nothing to do
      }
      else
      {
         bool isSuccess;

         EntryInfo_uninit(&this->entryInfo);

         isSuccess = MetadataTk_getRootEntryInfoCopy(app, &this->entryInfo);
         if (isSuccess)
            FhgfsOps_setHasRootEntryInfo(sb, true);
      }

      RWLock_writeUnlock(&this->entryInfoLock); // Write-UNLOCK

   }

}

/*
 * Get an EntryInfo read-lock.
 *
 * Note: Must be taken on reading parentEntryID. Should not be taken if only entryID is read,
 *       as entryID is never updated.
 *       See FhgfsInode_updateEntryInfoUnlocked() and FhgfsInode_updateEntryInfoOnRenameUnlocked()
 * Note: If the root inode is not initialized, it will be initialized by this method under
 *       a write-lock.
 */
void FhgfsInode_entryInfoReadLock(FhgfsInode* this)
{
   _FhgfsInode_initRootEntryInfo(this); // NOTE: might temporarily writelock entryInfoLock

   RWLock_readLock(&this->entryInfoLock); // Read-LOCK
}

void FhgfsInode_entryInfoReadUnlock(FhgfsInode* this)
{
   RWLock_readUnlock(&this->entryInfoLock);
}

/**
 * Note: Also might initialize the root-inode.
 */
void FhgfsInode_entryInfoWriteLock(FhgfsInode* this)
{
   _FhgfsInode_initRootEntryInfo(this); // NOTE: might temporarily writelock entryInfoLock

   RWLock_writeLock(&this->entryInfoLock); // Write-LOCK
}

void FhgfsInode_entryInfoWriteUnlock(FhgfsInode* this)
{
   RWLock_writeUnlock(&this->entryInfoLock);
}



/**
 * Compares given entryID with the entryID of this inode.
 *
 * Note: This is intended to be called from methods like __FhgfsOps_compareInodeID, which is
 * called e.g. by ilookup5 with the inode_hash_lock held, so this method may not sleep
 *
 * @return true if given ID matches, false otherwise
 */
bool FhgfsInode_compareEntryID(FhgfsInode* this, const char* otherEntryID)
{
   if(this->vfs_inode.i_ino == BEEGFS_INODE_ROOT_INO)
   { // root node => owner info is not stored in the inode
      return !strcmp(otherEntryID, META_ROOTDIR_ID_STR);
   }
   else
   {
      const char* entryID;

      entryID = this->entryInfo.entryID;

      if (unlikely(!entryID) )
      {
         printk_fhgfs(KERN_WARNING, "Bug: entryID is a NULL pointer!\n");
         return false;
      }

      return !strcmp(otherEntryID, entryID);
   }
}

/**
 * Return the DirEntryType of the given inode
 */
DirEntryType FhgfsInode_getDirEntryType(FhgfsInode* this)
{
   return this->entryInfo.entryType;
}

/**
 * Update EntryInfo on rename
 *
 * Note: newParentInfo and newName are not owned by this object!
 *
 * Note: FhgfsInode->entryInfoLock already needs to be write-locked by the caller.
 */
void FhgfsInode_updateEntryInfoOnRenameUnlocked(FhgfsInode* this, const EntryInfo* newParentInfo,
   const char* newName)
{
   EntryInfo_updateParentEntryID(&this->entryInfo, newParentInfo->entryID);
   EntryInfo_updateFileName(&this->entryInfo, newName);

   if (!DirEntryType_ISDIR(this->entryInfo.entryType) )
   {  // only update the ownerNodeID if it is not a directory
      this->entryInfo.owner = newParentInfo->owner;
      if (this->entryInfo.owner.isGroup)
         this->entryInfo.featureFlags |= ENTRYINFO_FEATURE_BUDDYMIRRORED;
      else
         this->entryInfo.featureFlags &= ~ENTRYINFO_FEATURE_BUDDYMIRRORED;
   }
}

/**
 * Update an existing EntryInfo with a new one
 *
 * Note: this->entryInfoLock already needs to be write-locked by the caller.
 */
void FhgfsInode_updateEntryInfoUnlocked(FhgfsInode* this, const EntryInfo* newEntryInfo)
{
   EntryInfo_update(&this->entryInfo, newEntryInfo);
}


int FhgfsInode_getStripeCount(FhgfsInode* this)
{
   return UInt16Vec_length(this->pattern->getStripeTargetIDs(this->pattern));
}

StripePattern* FhgfsInode_getStripePattern(FhgfsInode* this)
{
   return this->pattern;
}

/**
 * Converts FileHandleType to OPENFILE_ACCESS_... fhgfs flags.
 *
 * Note: This is especially required when a handle was referenced with allowRWHandle, because then
 * you don't know the OPENFILE_ACCESS_... flags.
 */
int FhgfsInode_handleTypeToOpenFlags(FileHandleType handleType)
{
   switch(handleType)
   {
      case FileHandleType_WRITE:
      {
         return OPENFILE_ACCESS_WRITE;
      } break;

      case FileHandleType_RW:
      {
         return OPENFILE_ACCESS_READWRITE;
      } break;

      default:
      {
         return OPENFILE_ACCESS_READ;
      } break;
   }
}

/**
 * Invalidate the cache of an inode
 */
void FhgfsInode_invalidateCache(FhgfsInode* this)
{
   Time_setZero(&this->dataCacheTime);
}

void FhgfsInode_incNumDirtyPages(FhgfsInode* this)
{
   AtomicInt64_inc(&this->numPageCacheDirtyPages);
}

void FhgfsInode_decNumDirtyPages(FhgfsInode* this)
{
   AtomicInt64_dec(&this->numPageCacheDirtyPages);
}

uint64_t FhgfsInode_getNumDirtyPages(FhgfsInode* this)
{
   return AtomicInt64_read(&this->numPageCacheDirtyPages);
}

void FhgfsInode_setNumDirtyPages(FhgfsInode* this, uint64_t value)
{
   return AtomicInt64_set(&this->numPageCacheDirtyPages, value);
}

/**
 * Test whether a given file inode is currently write-opened.
 *
 * @return true if the file is currently open.
 */
bool FhgfsInode_getHasDirtyPages(FhgfsInode* this)
{
   return !!FhgfsInode_getNumDirtyPages(this);
}

void FhgfsInode_setParentNodeID(FhgfsInode* this, NumNodeID parentNodeID)
{
   this->parentNodeID = parentNodeID;
}

NumNodeID FhgfsInode_getParentNodeID(FhgfsInode* this)
{
   return this->parentNodeID;
}

void FhgfsInode_incWriteBackCounter(FhgfsInode* this)
{
   AtomicInt_inc(&this->writeBackCounter);
}

void FhgfsInode_decWriteBackCounter(FhgfsInode* this)
{
   AtomicInt_dec(&this->writeBackCounter);
}

/**
 * @param this: Will be NULL on lookup calls
 */
int FhgfsInode_getWriteBackCounter(FhgfsInode* this)
{
   if (!this)
      return 0;

   return AtomicInt_read(&this->writeBackCounter);
}

void FhgfsInode_setNoIsizeDecrease(FhgfsInode* this)
{
   AtomicInt_set(&this->noRemoteIsizeDecrease, 1);
}

/**
 * Unset noRemoteIsizeDecrease if appropriate
 */
void FhgfsInode_unsetNoIsizeDecrease(FhgfsInode* this)
{
   struct address_space *mapping = this->vfs_inode.i_mapping;

   if (!FhgfsInode_getNoIsizeDecrease(this) )
      return; // not set, so no need to do anything

   if (FhgfsInode_getWriteBackCounter(this) )
      return; // still write-back threads left

   // check if there are dirty, writeback or mmapped-write pages
   if ( (FhgfsInode_getHasDirtyPages(this) )                ||
        (mapping_tagged(mapping, PAGECACHE_TAG_DIRTY) )     ||
        (mapping_tagged(mapping, PAGECACHE_TAG_WRITEBACK) ) ||
        (mapping_writably_mapped(mapping) ) )
    {  // server did not receive all pages yet
       return;
    }

   AtomicInt_set(&this->noRemoteIsizeDecrease, 0);
}

int FhgfsInode_getNoIsizeDecrease(FhgfsInode* this)
{
   return AtomicInt_read(&this->noRemoteIsizeDecrease);
}

void FhgfsInode_setLastWriteBackOrIsizeWriteTime(FhgfsInode* this)
{
   AtomicInt64_set(&this->lastWriteBackEndOrIsizeWriteTime, get_jiffies_64() );
}

uint64_t FhgfsInode_getLastWriteBackOrIsizeWriteTime(FhgfsInode* this)
{
   return AtomicInt64_read(&this->lastWriteBackEndOrIsizeWriteTime);
}

/**
 * Initialize iSizeHints
 *
 * @param this might be a NULL pointer
 *
 * Note: As optimization it will only read atomic values and jiffies if the inode refers to
 *       a regular file. As this is not known before doing lookup-stat, this might be a NULL
 *       pointer.
 */
void FhgfsInode_initIsizeHints(FhgfsInode* this, FhgfsIsizeHints* outISizeHints)
{
   struct address_space *mapping;

   if (this && !S_ISREG(this->vfs_inode.i_mode) )
      return;

   outISizeHints->timeBeforeRemoteStat = get_jiffies_64();
   outISizeHints->ignoreIsize = false;

   /* Check if there is a hint that we should ignore the remote isize */
   if (this)
   {
      mapping = this->vfs_inode.i_mapping;
      if ((FhgfsInode_getWriteBackCounter(this) || FhgfsInode_getHasDirtyPages(this)   ||
          (mapping_tagged(mapping, PAGECACHE_TAG_DIRTY) )                              ||
          (mapping_tagged(mapping, PAGECACHE_TAG_WRITEBACK) )                          ||
          (mapping_writably_mapped(mapping) ) ) )
      {
         outISizeHints->ignoreIsize = true;
      }
   }
}

/**
 * Set the page-write flag (there was a page write to this inode
 */
void FhgfsInode_setPageWriteFlag(FhgfsInode* this)
{
   /* Note: This is called for each an every page (in paged mode, mmap, splice, ...)
    *       and therefore a very hot path and therefore needs to be optimized.
    *       Mostly the read will succeed if there are paged writes and the read will only fail
    *       for the first inode. And as reading values is cheaper than writing values, we first
    *       check if the flag is already set */
   if (!FhgfsInode_getHasPageWriteFlag(this) )
      this->flags |= BEEGFS_INODE_FLAG_PAGED;
}

/**
 * Get the page-write flag
 *
 * Note: In order to avoid inode locking overhead, this is usually called without hold i_lock
 *       It is not perfectly reliable then, but at some point CPUs get synchronized and that
 *       is sufficient for us.
 */
int FhgfsInode_getHasPageWriteFlag(FhgfsInode* this)
{
   return this->flags & BEEGFS_INODE_FLAG_PAGED;
}

/**
 * There was an error writing pages of this inode
 */
void FhgfsInode_setWritePageError(FhgfsInode* this)
{
   this->flags |= BEEGFS_INODE_FLAG_WRITE_ERROR;
}

/**
 * Clear the write error
 */
void FhgfsInode_clearWritePageError(FhgfsInode* this)
{
   this->flags &= ~(BEEGFS_INODE_FLAG_WRITE_ERROR);
}

int FhgfsInode_getHasWritePageError(FhgfsInode* this)
{
   return this->flags & BEEGFS_INODE_FLAG_WRITE_ERROR;
}

#endif /* FHGFSINODE_H_ */
