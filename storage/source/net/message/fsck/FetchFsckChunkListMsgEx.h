#ifndef STORAGE_FETCHFSCKCHUNKSMSGEX_H
#define STORAGE_FETCHFSCKCHUNKSMSGEX_H

#include <common/net/message/fsck/FetchFsckChunkListMsg.h>
#include <common/net/message/fsck/FetchFsckChunkListRespMsg.h>

class FetchFsckChunkListMsgEx : public FetchFsckChunkListMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* STORAGE_FETCHFSCKCHUNKSMSGEX_H */
