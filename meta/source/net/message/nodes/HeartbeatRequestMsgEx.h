#pragma once

#include <common/net/message/nodes/HeartbeatRequestMsg.h>

class HeartbeatRequestMsgEx : public HeartbeatRequestMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

