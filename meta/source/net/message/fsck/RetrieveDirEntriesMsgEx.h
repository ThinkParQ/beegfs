#pragma once

#include <common/net/message/fsck/RetrieveDirEntriesMsg.h>
#include <common/net/message/fsck/RetrieveDirEntriesRespMsg.h>

class RetrieveDirEntriesMsgEx : public RetrieveDirEntriesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

