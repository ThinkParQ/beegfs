#pragma once

#include <common/net/message/fsck/CheckAndRepairDupInodeMsg.h>

class CheckAndRepairDupInodeMsgEx : public CheckAndRepairDupInodeMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

