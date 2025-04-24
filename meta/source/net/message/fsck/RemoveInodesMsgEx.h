#pragma once

#include <common/net/message/fsck/RemoveInodesMsg.h>

class RemoveInodesMsgEx : public RemoveInodesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

