#pragma once

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/MoveChunkFileMsg.h>
#include <common/net/message/fsck/MoveChunkFileRespMsg.h>

class MoveChunkFileMsgEx : public MoveChunkFileMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      unsigned moveChunk();
};

