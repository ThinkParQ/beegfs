#ifndef MKLOCALDIRMSGEX_H_
#define MKLOCALDIRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MkLocalDirMsg.h>
#include <storage/MetaStore.h>

class MkLocalDirMsgEx : public MkLocalDirMsg
{
   public:
      MkLocalDirMsgEx() : MkLocalDirMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr mkLocalDir(StripePattern* pattern);
   
};

#endif /*MKLOCALDIRMSGEX_H_*/
