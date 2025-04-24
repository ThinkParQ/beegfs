#pragma once

#include <common/net/message/storage/mirroring/SetLastBuddyCommOverrideMsg.h>
#include <common/storage/StorageErrors.h>

class SetLastBuddyCommOverrideMsgEx : public SetLastBuddyCommOverrideMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

