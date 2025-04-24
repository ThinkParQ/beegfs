#pragma once

#include <common/net/message/nodes/RefreshTargetStatesMsg.h>


class RefreshTargetStatesMsgEx : public RefreshTargetStatesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

