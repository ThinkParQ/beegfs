#pragma once

#include <common/net/message/fsck/FetchFsckChunkListMsg.h>
#include <common/net/message/fsck/FetchFsckChunkListRespMsg.h>

class FetchFsckChunkListMsgEx : public FetchFsckChunkListMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

