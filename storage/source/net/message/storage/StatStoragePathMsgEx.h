#ifndef STATSTORAGEPATHMSGEX_H_
#define STATSTORAGEPATHMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/StatStoragePathMsg.h>

// stat of the path to the storage directory, result is similar to statfs

class StatStoragePathMsgEx : public StatStoragePathMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr statStoragePath(int64_t* outSizeTotal, int64_t* outSizeFree,
         int64_t* outInodesTotal, int64_t* outInodesFree);
};


#endif /*STATSTORAGEPATHMSGEX_H_*/
