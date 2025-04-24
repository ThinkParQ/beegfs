#pragma once

#include <common/net/message/control/AuthenticateChannelMsg.h>


class AuthenticateChannelMsgEx : public AuthenticateChannelMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


