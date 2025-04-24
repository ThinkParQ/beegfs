#pragma once

#include <common/net/message/nodes/StorageBenchControlMsg.h>
#include <common/Common.h>

class StorageBenchControlMsgEx: public StorageBenchControlMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

