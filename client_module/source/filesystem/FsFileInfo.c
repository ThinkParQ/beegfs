#include <common/storage/striping/StripePattern.h>
#include <filesystem/FsFileInfo.h>
#include <common/toolkit/StringTk.h>


struct FsFileInfo
{
   FsObjectInfo fsObjectInfo;

   unsigned accessFlags;
   FileHandleType handleType;
   bool appending;

   ssize_t cacheHits; // hits/misses counter (min and max limited by thresholds)
   bool allowCaching; // false when O_DIRECT or caching disabled in config
   loff_t lastReadOffset; // offset after last read (to decide if IO would be a cache hit)
   loff_t lastWriteOffset; // offset after last write (to decide if IO would be a cache hit)

   bool usedEntryLocking; // true when entry lock methods were used (needed for cleanup)
};


/**
 * @param accessFlags fhgfs access flags (not OS flags)
 */
void FsFileInfo_init(FsFileInfo* this, App* app, unsigned accessFlags, FileHandleType handleType)
{
   FsObjectInfo_init( (FsObjectInfo*)this, app, FsObjectType_FILE);

   this->accessFlags = accessFlags;
   this->handleType = handleType;
   this->appending = false;

   this->cacheHits = FSFILEINFO_CACHE_HITS_INITIAL;
   this->allowCaching = true;
   this->lastReadOffset = 0;
   this->lastWriteOffset = 0;

   this->usedEntryLocking = false;

   // assign virtual functions
   ( (FsObjectInfo*)this)->uninit = FsFileInfo_uninit;
}

/**
 * @param accessFlags fhgfs access flags (not OS flags)
 */
struct FsFileInfo* FsFileInfo_construct(App* app, unsigned accessFlags, FileHandleType handleType)
{
   struct FsFileInfo* this = (FsFileInfo*)os_kmalloc(sizeof(*this) );

   if(likely(this) )
      FsFileInfo_init(this, app, accessFlags, handleType);

   return this;
}

void FsFileInfo_uninit(FsObjectInfo* this)
{
}

/**
 * Increase cache hits counter.
 *
 * Note: Hits counter won't get higher than a certain threshold.
 */
void FsFileInfo_incCacheHits(FsFileInfo* this)
{
   if(this->cacheHits < FSFILEINFO_CACHE_HITS_THRESHOLD)
      this->cacheHits++;
}

/**
 * Decrease cache hits counter.
 *
 * Note: Hits counter won't get lower than a certain threshold.
 */
void FsFileInfo_decCacheHits(FsFileInfo* this)
{
   if(this->cacheHits > FSFILEINFO_CACHE_MISS_THRESHOLD)
      this->cacheHits--;
}


unsigned FsFileInfo_getAccessFlags(FsFileInfo* this)
{
   return this->accessFlags;
}

FileHandleType FsFileInfo_getHandleType(FsFileInfo* this)
{
   return this->handleType;
}


void FsFileInfo_setAppending(FsFileInfo* this, bool appending)
{
   this->appending = appending;
}

bool FsFileInfo_getAppending(FsFileInfo* this)
{
   return this->appending;
}

ssize_t FsFileInfo_getCacheHits(FsFileInfo* this)
{
   return this->cacheHits;
}

void FsFileInfo_setAllowCaching(FsFileInfo* this, bool allowCaching)
{
   this->allowCaching = allowCaching;
}

bool FsFileInfo_getAllowCaching(FsFileInfo* this)
{
   return this->allowCaching;
}

void FsFileInfo_setLastReadOffset(FsFileInfo* this, loff_t lastReadOffset)
{
   this->lastReadOffset = lastReadOffset;
}

loff_t FsFileInfo_getLastReadOffset(FsFileInfo* this)
{
   return this->lastReadOffset;
}

void FsFileInfo_setLastWriteOffset(FsFileInfo* this, loff_t lastWriteOffset)
{
   this->lastWriteOffset = lastWriteOffset;
}

loff_t FsFileInfo_getLastWriteOffset(FsFileInfo* this)
{
   return this->lastWriteOffset;
}

void FsFileInfo_getIOInfo(FsFileInfo* this, struct FhgfsInode* fhgfsInode,
   struct RemotingIOInfo* outIOInfo)
{
   FhgfsInode_getRefIOInfo(fhgfsInode, this->handleType, this->accessFlags, outIOInfo);
}

void FsFileInfo_setUsedEntryLocking(FsFileInfo* this)
{
   this->usedEntryLocking = true;
}

bool FsFileInfo_getUsedEntryLocking(FsFileInfo* this)
{
   return this->usedEntryLocking;
}

