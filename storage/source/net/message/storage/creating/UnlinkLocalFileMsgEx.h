#pragma once

#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>

class StorageTarget;

class UnlinkLocalFileMsgEx : public UnlinkLocalFileMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      int getTargetFD(const StorageTarget& target, ResponseContext& ctx, bool* outResponseSent);
      FhgfsOpsErr forwardToSecondary(StorageTarget& target, ResponseContext& ctx,
            bool* outChunkLocked);
};

