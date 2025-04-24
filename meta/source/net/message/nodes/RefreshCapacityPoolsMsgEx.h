#pragma once

#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>


class RefreshCapacityPoolsMsgEx : public RefreshCapacityPoolsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

