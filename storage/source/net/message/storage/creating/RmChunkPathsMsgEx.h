#pragma once

#include <common/net/message/storage/creating/RmChunkPathsMsg.h>

class RmChunkPathsMsgEx : public RmChunkPathsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

