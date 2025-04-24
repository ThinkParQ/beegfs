#pragma once

#include <common/net/message/session/FSyncLocalFileMsg.h>

class FSyncLocalFileMsgEx : public FSyncLocalFileMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr fsync();
};

