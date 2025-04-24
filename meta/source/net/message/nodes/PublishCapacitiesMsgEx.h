#pragma once

#include <common/net/message/nodes/PublishCapacitiesMsg.h>


class PublishCapacitiesMsgEx : public PublishCapacitiesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


