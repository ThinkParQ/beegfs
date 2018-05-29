#include <common/Common.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckFileInode.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/EntryInfo.h>

#ifndef FSCKTK_H_
#define FSCKTK_H_

enum FsckStripePatternType
{
   FsckStripePatternType_INVALID = 0, FsckStripePatternType_RAID0 = 1,
   FsckStripePatternType_BUDDYMIRROR = 2
};

enum FetchFsckChunkListStatus
{
   FetchFsckChunkListStatus_NOTSTARTED=0, /* */
   FetchFsckChunkListStatus_RUNNING=1, /* nothing strange so far */
   FetchFsckChunkListStatus_FINISHED=2, /* all chunks are read */
   FetchFsckChunkListStatus_READERROR=3, /* a read error occured */
};

class FsckTk
{
   public:
      FsckTk(){ };
      virtual ~FsckTk(){ };

      static FsckStripePatternType stripePatternToFsckStripePattern(StripePattern* stripePattern,
         unsigned *outChunkSize = NULL, UInt16Vector* outTargetIDVector = NULL);
      static StripePattern* FsckStripePatternToStripePattern(
         FsckStripePatternType stripePatternType, unsigned chunkSize, UInt16Vector* targetIDVector);

      static FsckDirEntryType DirEntryTypeToFsckDirEntryType(DirEntryType entryType);
};

#endif /* FSCKTK_H_ */
