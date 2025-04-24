#pragma once

#include <common/net/message/nodes/RemoveNodeMsg.h>

class RemoveNodeMsgEx : public RemoveNodeMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


