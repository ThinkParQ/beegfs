#ifndef REMOTINGIOINFO_H_
#define REMOTINGIOINFO_H_

#include <common/Common.h>
#include <common/storage/PathInfo.h>
#include <common/storage/striping/StripePattern.h>
#include <toolkit/BitStore.h>
#include <app/App.h>


struct RemotingIOInfo;
typedef struct RemotingIOInfo RemotingIOInfo;



// inliners
static inline void RemotingIOInfo_initOpen(App* app, unsigned accessFlags,
   AtomicInt* pMaxUsedTargetIndex, PathInfo* pathInfo, RemotingIOInfo* outIOInfo);
static inline void RemotingIOInfo_initSpecialClose(App* app, const char* fileHandleID,
   AtomicInt* maxUsedTargetIndex, unsigned accessFlags, RemotingIOInfo* outIOInfo);
static inline void RemotingIOInfo_freeVals(RemotingIOInfo* outIOInfo);
static inline size_t RemotingIOInfo_getNumPagesPerStripe(RemotingIOInfo* ioInfo);
static inline size_t RemotingIOInfo_getNumPagesPerChunk(RemotingIOInfo* ioInfo);



struct RemotingIOInfo
{
      App* app;

      const char* fileHandleID;
      struct StripePattern* pattern;
      PathInfo* pathInfo;
      unsigned accessFlags; // OPENFILE_ACCESS_... flags

      // pointers to fhgfsInode->fileHandles[handleType]...

      bool* needsAppendLockCleanup; // (note: can be NULL in some cases, implies "false")
      AtomicInt* maxUsedTargetIndex;
      BitStore* firstWriteDone;

      unsigned userID;     // only used in storage server write message
      unsigned groupID;    // only used in storage server write message
#ifdef BEEGFS_NVFS
      bool nvfs;
#endif
};


/**
 * Prepares a RemotingIOInfo for Remoting_openfile().
 *
 * Note: Be careful with this. This is only useful in very special cases (e.g. stateless file IO,
 * when you call FhgfsOpsRemoting_openfile() directly). Some values like userID also need to be
 * set manually in these cases.
 * You will typically rather go through the inode and thus use FhgfsInode_referenceHandle().
 *
 * @param accessFlags OPENFILE_ACCESS_... flags
 * @param maxUsedTargetIndex may be NULL for open, but in case you will use the ioInfo also for
 * IO, you will probably want to set it.
 */
void RemotingIOInfo_initOpen(App* app, unsigned accessFlags, AtomicInt* maxUsedTargetIndex,
   PathInfo* pathInfo, RemotingIOInfo* outIOInfo)
{
   outIOInfo->app = app;

   outIOInfo->fileHandleID = NULL;
   outIOInfo->pattern = NULL;
   outIOInfo->pathInfo = pathInfo;
   outIOInfo->accessFlags = accessFlags;

   outIOInfo->needsAppendLockCleanup = NULL;
   outIOInfo->maxUsedTargetIndex = maxUsedTargetIndex;
   outIOInfo->firstWriteDone = NULL;

   outIOInfo->userID = 0;
   outIOInfo->groupID = 0;
#ifdef BEEGFS_NVFS
   outIOInfo->nvfs = false;
#endif
}


/**
 * Initialize RemotingIOInfo for close only. This is used for very special failures in
 * atomic_open and lookup_open.
 */
void RemotingIOInfo_initSpecialClose(App* app, const char* fileHandleID,
   AtomicInt* maxUsedTargetIndex, unsigned accessFlags, RemotingIOInfo* outIOInfo)
{
   outIOInfo->app = app;

   outIOInfo->fileHandleID = fileHandleID;
   outIOInfo->pattern = NULL;
   outIOInfo->pathInfo = NULL;
   outIOInfo->accessFlags = accessFlags;

   outIOInfo->needsAppendLockCleanup = NULL;
   outIOInfo->maxUsedTargetIndex = maxUsedTargetIndex;
   outIOInfo->firstWriteDone = NULL;

   outIOInfo->userID = 0;
   outIOInfo->groupID = 0;
#ifdef BEEGFS_NVFS
   outIOInfo->nvfs = false;
#endif
}

/**
 * Note: Be careful with this. This is only useful in very special cases (e.g. stateless file IO,
 * when you called Remoting_openfile() directly). You will typically rather use the
 * FhgfsInode_referenceHandle() & Co routines.
 */
void RemotingIOInfo_freeVals(RemotingIOInfo* outIOInfo)
{
   SAFE_DESTRUCT(outIOInfo->pattern, StripePattern_virtualDestruct);
   SAFE_KFREE(outIOInfo->fileHandleID);

   if (outIOInfo->pathInfo)
      PathInfo_uninit(outIOInfo->pathInfo);
}


/**
 * Return the number of pages per stripe
 */
size_t RemotingIOInfo_getNumPagesPerStripe(RemotingIOInfo* ioInfo)
{
   StripePattern* pattern = ioInfo->pattern;
   UInt16Vec* targetIDs = pattern->getStripeTargetIDs(pattern);
   size_t numStripeNodes = UInt16Vec_length(targetIDs);

   return RemotingIOInfo_getNumPagesPerChunk(ioInfo) * numStripeNodes;
}

size_t RemotingIOInfo_getNumPagesPerChunk(RemotingIOInfo* ioInfo)
{
   StripePattern* pattern = ioInfo->pattern;
   unsigned chunkSize = StripePattern_getChunkSize(pattern);

   return chunkSize / PAGE_SIZE;
}

#endif /* REMOTINGIOINFO_H_ */
