#pragma once

#include <common/net/message/fsck/RetrieveFsIDsMsg.h>
#include <common/net/message/fsck/RetrieveFsIDsRespMsg.h>

class RetrieveFsIDsMsgEx : public RetrieveFsIDsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

