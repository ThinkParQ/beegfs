#pragma once

#include <common/net/message/nodes/GetNodesMsg.h>

class GetNodesMsgEx : public GetNodesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

