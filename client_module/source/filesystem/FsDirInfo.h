#ifndef FSDIRINFO_H_
#define FSDIRINFO_H_

#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/vector/Int64CpyVec.h>
#include <common/toolkit/vector/StrCpyVec.h>
#include <common/toolkit/vector/UInt8Vec.h>
#include <common/Common.h>
#include "FsObjectInfo.h"


struct FsDirInfo;
typedef struct FsDirInfo FsDirInfo;


static inline void FsDirInfo_init(FsDirInfo* this, App* app);
static inline FsDirInfo* FsDirInfo_construct(App* app);
static inline void FsDirInfo_uninit(FsObjectInfo* this);

// getters & setters
static inline loff_t FsDirInfo_getServerOffset(FsDirInfo* this);
static inline  void FsDirInfo_setServerOffset(FsDirInfo* this, int64_t serverOffset);
static inline size_t FsDirInfo_getCurrentContentsPos(FsDirInfo* this);
static inline void FsDirInfo_setCurrentContentsPos(FsDirInfo* this, size_t currentContentsPos);
static inline struct StrCpyVec* FsDirInfo_getDirContents(FsDirInfo* this);
static inline struct StrCpyVec* FsDirInfo_getEntryIDs(FsDirInfo* this);
static inline struct UInt8Vec* FsDirInfo_getDirContentsTypes(FsDirInfo* this);
static inline struct Int64CpyVec* FsDirInfo_getServerOffsets(FsDirInfo* this);
static inline void FsDirInfo_setEndOfDir(FsDirInfo* this, bool endOfDir);
static inline bool FsDirInfo_getEndOfDir(FsDirInfo* this);


struct FsDirInfo
{
   FsObjectInfo fsObjectInfo;

   StrCpyVec dirContents; // entry names
   UInt8Vec dirContentsTypes; // DirEntryType elements matching dirContents vector
   StrCpyVec entryIDs; // entryID elements matching dirContents vector
   Int64CpyVec serverOffsets; // dir entry offsets for telldir() matching dirContents vector
   int64_t serverOffset; /* offset for the next incremental list request to the server
                            (equals last element of serverOffsets vector) */
   size_t currentContentsPos; // current local pos in dirContents (>=0 && <dirContents_len)
   bool endOfDir; // true if server reached end of dir entries during last query
};



void FsDirInfo_init(FsDirInfo* this, App* app)
{
   FsObjectInfo_init( (FsObjectInfo*)this, app, FsObjectType_DIRECTORY);

   StrCpyVec_init(&this->dirContents);
   UInt8Vec_init(&this->dirContentsTypes);
   StrCpyVec_init(&this->entryIDs);
   Int64CpyVec_init(&this->serverOffsets);

   this->serverOffset = 0;
   this->currentContentsPos = 0;
   this->endOfDir = false;

   // assign virtual functions
   ( (FsObjectInfo*)this)->uninit = FsDirInfo_uninit;
}

struct FsDirInfo* FsDirInfo_construct(App* app)
{
   struct FsDirInfo* this = (FsDirInfo*)os_kmalloc(sizeof(*this) );

   if(likely(this) )
      FsDirInfo_init(this, app);

   return this;
}

void FsDirInfo_uninit(FsObjectInfo* this)
{
   FsDirInfo* thisCast = (FsDirInfo*)this;

   StrCpyVec_uninit(&thisCast->dirContents);
   UInt8Vec_uninit(&thisCast->dirContentsTypes);
   StrCpyVec_uninit(&thisCast->entryIDs);
   Int64CpyVec_uninit(&thisCast->serverOffsets);
}

loff_t FsDirInfo_getServerOffset(FsDirInfo* this)
{
   return this->serverOffset;
}

void FsDirInfo_setServerOffset(FsDirInfo* this, int64_t serverOffset)
{
   this->serverOffset = serverOffset;
}

size_t FsDirInfo_getCurrentContentsPos(FsDirInfo* this)
{
   return this->currentContentsPos;
}

void FsDirInfo_setCurrentContentsPos(FsDirInfo* this, size_t currentContentsPos)
{
   this->currentContentsPos = currentContentsPos;
}

StrCpyVec* FsDirInfo_getDirContents(FsDirInfo* this)
{
   return &this->dirContents;
}

StrCpyVec* FsDirInfo_getEntryIDs(FsDirInfo* this)
{
   return &this->entryIDs;
}

/**
 * @return vector of DirEntryType elements, matching dirContents vector
 */
UInt8Vec* FsDirInfo_getDirContentsTypes(FsDirInfo* this)
{
   return &this->dirContentsTypes;
}

Int64CpyVec* FsDirInfo_getServerOffsets(FsDirInfo* this)
{
   return &this->serverOffsets;
}

void FsDirInfo_setEndOfDir(FsDirInfo* this, bool endOfDir)
{
   this->endOfDir = endOfDir;
}

bool FsDirInfo_getEndOfDir(FsDirInfo* this)
{
   return this->endOfDir;
}



#endif /*FSDIRINFO_H_*/
