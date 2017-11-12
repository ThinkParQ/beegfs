#ifndef RECREATEDENTRIESMSGEX_H
#define RECREATEDENTRIESMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/RecreateDentriesMsg.h>
#include <common/net/message/fsck/RecreateDentriesRespMsg.h>

class RecreateDentriesMsgEx : public RecreateDentriesMsg
{
   public:
      RecreateDentriesMsgEx() : RecreateDentriesMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
         size_t bufLen, HighResolutionStats* stats);

   private:

};

#endif /*RECREATEDENTRIESMSGEX_H*/
