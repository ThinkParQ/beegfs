#pragma once

#include <common/net/message/control/PeerInfoMsg.h>

class PeerInfoMsgEx : public PeerInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


