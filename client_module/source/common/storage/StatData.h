/*
 * Information provided by stat()
 */

#ifndef STATDATA_H_
#define STATDATA_H_

#include <common/storage/StorageDefinitions.h>
#include <common/FhgfsTypes.h>

#define STATDATA_FEATURE_SPARSE_FILE   1

// stat blocks are 512 bytes, ">> 9" is then the same as "/ 512", but faster
#define STATDATA_SIZETOBLOCKSBIT_SHIFT  9


struct StatData;
typedef struct StatData StatData;

extern bool StatData_deserialize(DeserializeCtx* ctx, StatData* outThis);

static inline void StatData_getOsStat(StatData* this, fhgfs_stat* outOsStat);

struct StatData
{
      unsigned flags;

      int64_t fileSize;
      uint64_t numBlocks;
      int64_t creationTimeSecs;     // real creation time
      int64_t attribChangeTimeSecs; // this corresponds to unix ctime
      unsigned nlink;

      SettableFileAttribs settableFileAttribs;

      unsigned contentsVersion; // inc'ed when file contents are modified (for cache invalidation)

};

void StatData_getOsStat(StatData* this, fhgfs_stat* outOsStat)
{
   outOsStat->mode   = this->settableFileAttribs.mode;
   outOsStat->size   = this->fileSize;
   outOsStat->blocks = this->numBlocks;

   outOsStat->uid    = this->settableFileAttribs.userID;
   outOsStat->gid    = this->settableFileAttribs.groupID;
   outOsStat->nlink  = this->nlink;

   outOsStat->atime.tv_sec   = this->settableFileAttribs.lastAccessTimeSecs;
   outOsStat->atime.tv_nsec  = 0;

   outOsStat->mtime.tv_sec   = this->settableFileAttribs.modificationTimeSecs;
   outOsStat->mtime.tv_nsec  = 0;

   outOsStat->ctime.tv_sec   = this->attribChangeTimeSecs;
   outOsStat->ctime.tv_nsec  = 0;
}

#endif /* STATDATA_H_ */
