#pragma once

#include <common/net/message/storage/mirroring/StorageResyncStartedMsg.h>

class StorageResyncStartedMsgEx : public StorageResyncStartedMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      void deleteMirrorSessions(uint16_t targetID);
};

