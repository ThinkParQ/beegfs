#pragma once

#include <common/net/message/nodes/storagepools/RefreshStoragePoolsMsg.h>

class RefreshStoragePoolsMsgEx : public RefreshStoragePoolsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

