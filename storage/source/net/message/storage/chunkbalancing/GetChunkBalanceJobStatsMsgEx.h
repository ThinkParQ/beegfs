#pragma once

#include <common/net/message/storage/chunkbalancing/GetChunkBalanceJobStatsMsg.h>
#include <common/storage/StorageErrors.h>

class GetChunkBalanceJobStatsMsgEx : public GetChunkBalanceJobStatsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

