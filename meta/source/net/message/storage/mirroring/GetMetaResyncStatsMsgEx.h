#pragma once

#include <common/net/message/storage/mirroring/GetMetaResyncStatsMsg.h>

class GetMetaResyncStatsMsgEx : public GetMetaResyncStatsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

