#pragma once

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/UpdateDirAttribsMsg.h>
#include <common/net/message/fsck/UpdateDirAttribsRespMsg.h>

class UpdateDirAttribsMsgEx : public UpdateDirAttribsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

