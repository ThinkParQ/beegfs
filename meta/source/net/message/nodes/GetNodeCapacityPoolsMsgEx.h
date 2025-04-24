#pragma once

#include <common/net/message/nodes/GetNodeCapacityPoolsMsg.h>

class GetNodeCapacityPoolsMsgEx : public GetNodeCapacityPoolsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

