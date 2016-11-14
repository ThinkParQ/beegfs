#ifndef RECREATEFSIDSMSGEX_H
#define RECREATEFSIDSMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/RecreateFsIDsMsg.h>
#include <common/net/message/fsck/RecreateFsIDsRespMsg.h>

class RecreateFsIDsMsgEx : public RecreateFsIDsMsg
{
   public:
      RecreateFsIDsMsgEx() : RecreateFsIDsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
         size_t bufLen, HighResolutionStats* stats);

   private:

};

#endif /*RECREATEFSIDSMSGEX_H*/
