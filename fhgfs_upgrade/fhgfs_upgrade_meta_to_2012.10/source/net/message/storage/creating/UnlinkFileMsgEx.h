#ifndef UNLINKFILEMSGEX_H_
#define UNLINKFILEMSGEX_H_

#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <storage/MetaStore.h>


class UnlinkFileMsgEx : public UnlinkFileMsg
{
   public:
      UnlinkFileMsgEx() : UnlinkFileMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
   
};

#endif /*UNLINKFILEMSGEX_H_*/
