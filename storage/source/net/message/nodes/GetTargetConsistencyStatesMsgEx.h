#pragma once

#include <common/net/message/nodes/GetTargetConsistencyStatesMsg.h>

class GetTargetConsistencyStatesMsgEx : public GetTargetConsistencyStatesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

