#ifndef GETSTORAGERESYNCSTATSMSGEX_H_
#define GETSTORAGERESYNCSTATSMSGEX_H_

#include <common/net/message/storage/mirroring/GetStorageResyncStatsMsg.h>
#include <common/storage/StorageErrors.h>

class GetStorageResyncStatsMsgEx : public GetStorageResyncStatsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*GETSTORAGERESYNCSTATSMSGEX_H_*/
