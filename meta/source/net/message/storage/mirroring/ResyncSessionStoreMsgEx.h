#pragma once

#include <common/net/message/storage/mirroring/ResyncSessionStoreMsg.h>

class ResyncSessionStoreMsgEx : public ResyncSessionStoreMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

