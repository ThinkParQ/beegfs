#pragma once

#include <common/net/message/nodes/MapTargetsMsg.h>

class MapTargetsMsgEx : public MapTargetsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


