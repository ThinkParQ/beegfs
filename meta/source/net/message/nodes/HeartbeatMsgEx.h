#pragma once

#include <common/net/message/nodes/HeartbeatMsg.h>

class HeartbeatMsgEx : public HeartbeatMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      void processIncomingRoot();
};

