#pragma once

#include <common/net/message/storage/chunkbalancing/GetChunkBalanceJobStatsMsg.h>

class GetChunkBalanceJobStatsMsgEx : public GetChunkBalanceJobStatsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

