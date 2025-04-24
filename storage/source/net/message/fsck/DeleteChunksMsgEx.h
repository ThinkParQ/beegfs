#pragma once

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/DeleteChunksMsg.h>
#include <common/net/message/fsck/DeleteChunksRespMsg.h>

class DeleteChunksMsgEx : public DeleteChunksMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

