#pragma once

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/SetLocalAttrMsg.h>

class StorageTarget;

class SetLocalAttrMsgEx : public SetLocalAttrMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      int getTargetFD(const StorageTarget& target, ResponseContext& ctx, bool* outResponseSent);
      FhgfsOpsErr forwardToSecondary(StorageTarget& target, ResponseContext& ctx,
            bool* outChunkLocked);
};


