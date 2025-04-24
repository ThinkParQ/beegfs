#pragma once

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/DeleteDirEntriesMsg.h>
#include <common/net/message/fsck/DeleteDirEntriesRespMsg.h>

class DeleteDirEntriesMsgEx : public DeleteDirEntriesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

