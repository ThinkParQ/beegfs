#pragma once

#include <common/net/message/storage/attribs/GetChunkFileAttribsMsg.h>

class StorageTarget;

class GetChunkFileAttribsMsgEx : public GetChunkFileAttribsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      int getTargetFD(const StorageTarget& target, ResponseContext& ctx, bool* outResponseSent);
};

