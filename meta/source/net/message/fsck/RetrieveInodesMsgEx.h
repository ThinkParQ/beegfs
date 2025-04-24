#pragma once

#include <common/net/message/fsck/RetrieveInodesMsg.h>
#include <common/net/message/fsck/RetrieveInodesRespMsg.h>

class RetrieveInodesMsgEx : public RetrieveInodesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

