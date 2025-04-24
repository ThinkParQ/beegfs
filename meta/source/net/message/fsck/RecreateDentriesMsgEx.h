#pragma once

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/RecreateDentriesMsg.h>
#include <common/net/message/fsck/RecreateDentriesRespMsg.h>

class RecreateDentriesMsgEx : public RecreateDentriesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

