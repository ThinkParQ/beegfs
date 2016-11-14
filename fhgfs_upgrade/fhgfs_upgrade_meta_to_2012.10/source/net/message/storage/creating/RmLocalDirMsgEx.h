#ifndef RMLOCALDIRMSGEX_H_
#define RMLOCALDIRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/RmLocalDirMsg.h>
#include <storage/MetaStore.h>


class RmLocalDirMsgEx : public RmLocalDirMsg
{
   public:
      RmLocalDirMsgEx() : RmLocalDirMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr rmDir(void);
};

#endif /*RMLOCALDIRMSGEX_H_*/
