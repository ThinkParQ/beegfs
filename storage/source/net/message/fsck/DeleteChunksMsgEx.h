#ifndef DELETECHUNKSMSGEX_H
#define DELETECHUNKSMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/DeleteChunksMsg.h>
#include <common/net/message/fsck/DeleteChunksRespMsg.h>

class DeleteChunksMsgEx : public DeleteChunksMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*DELETECHUNKSMSGEX_H*/
