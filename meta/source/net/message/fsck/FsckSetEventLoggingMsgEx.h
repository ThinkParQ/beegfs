#pragma once

#include <common/net/message/fsck/FsckSetEventLoggingMsg.h>
#include <common/net/message/fsck/FsckSetEventLoggingRespMsg.h>

class FsckSetEventLoggingMsgEx : public FsckSetEventLoggingMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

