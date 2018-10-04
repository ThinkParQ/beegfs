#ifndef FSYNCLOCALFILEMSGEX_H_
#define FSYNCLOCALFILEMSGEX_H_

#include <common/net/message/session/FSyncLocalFileMsg.h>

class FSyncLocalFileMsgEx : public FSyncLocalFileMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr fsync();
};

#endif /*FSYNCLOCALFILEMSGEX_H_*/
