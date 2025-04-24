#pragma once

#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>

class SetTargetConsistencyStatesMsgEx : public SetTargetConsistencyStatesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

