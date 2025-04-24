#pragma once

#include <common/net/message/nodes/GetTargetMappingsMsg.h>

class GetTargetMappingsMsgEx : public GetTargetMappingsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


