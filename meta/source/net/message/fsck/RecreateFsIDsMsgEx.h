#pragma once

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/RecreateFsIDsMsg.h>
#include <common/net/message/fsck/RecreateFsIDsRespMsg.h>

class RecreateFsIDsMsgEx : public RecreateFsIDsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

