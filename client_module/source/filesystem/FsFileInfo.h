#ifndef FSFILEINFO_H_
#define FSFILEINFO_H_

#include <common/Common.h>
#include <filesystem/FhgfsInode.h>
#include "FsObjectInfo.h"


#define FSFILEINFO_CACHE_HITS_INITIAL       (3) /* some cache-optimistic initial hits value */
#define FSFILEINFO_CACHE_HITS_THRESHOLD     (5) /* hits won't ever get higher than this number */
#define FSFILEINFO_CACHE_MISS_THRESHOLD     (-5) /* hits won't ever get lower than this number */
#define FSFILEINFO_CACHE_SLOWSTART_READLEN  (64*1024) /* smaller read-ahead for offset==0, e.g. if
                                                         some process looks only at file starts */


enum FileBufferType; // forward declaration
struct StripePattern; // forward declaration
struct FhgfsInode; // forward declaration
struct RemotingIOInfo; // forward declaration

struct FsFileInfo;
typedef struct FsFileInfo FsFileInfo;


extern void FsFileInfo_init(FsFileInfo* this, App* app, unsigned accessFlags,
   FileHandleType handleType);
extern FsFileInfo* FsFileInfo_construct(App* app, unsigned accessFlags, FileHandleType handleType);
extern void FsFileInfo_uninit(FsObjectInfo* this);

extern void FsFileInfo_incCacheHits(FsFileInfo* this);
extern void FsFileInfo_decCacheHits(FsFileInfo* this);

// getters & setters
extern unsigned FsFileInfo_getAccessFlags(FsFileInfo* this);
extern FileHandleType FsFileInfo_getHandleType(FsFileInfo* this);
extern void FsFileInfo_setAppending(FsFileInfo* this, bool appending);
extern bool FsFileInfo_getAppending(FsFileInfo* this);
extern ssize_t FsFileInfo_getCacheHits(FsFileInfo* this);
extern void FsFileInfo_setAllowCaching(FsFileInfo* this, bool allowCaching);
extern bool FsFileInfo_getAllowCaching(FsFileInfo* this);
extern void FsFileInfo_setLastReadOffset(FsFileInfo* this, loff_t lastReadOffset);
extern loff_t FsFileInfo_getLastReadOffset(FsFileInfo* this);
extern void FsFileInfo_setLastWriteOffset(FsFileInfo* this, loff_t lastWriteOffset);
extern loff_t FsFileInfo_getLastWriteOffset(FsFileInfo* this);
extern void FsFileInfo_getIOInfo(FsFileInfo* this, struct FhgfsInode* fhgfsInode,
   struct RemotingIOInfo* outIOInfo);
extern void FsFileInfo_setUsedEntryLocking(FsFileInfo* this);
extern bool FsFileInfo_getUsedEntryLocking(FsFileInfo* this);


#endif /*FSFILEINFO_H_*/
