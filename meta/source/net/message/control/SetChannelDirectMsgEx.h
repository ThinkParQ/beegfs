#pragma once

#include <common/net/message/control/SetChannelDirectMsg.h>

class SetChannelDirectMsgEx : public SetChannelDirectMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


