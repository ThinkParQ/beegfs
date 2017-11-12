#ifndef FIXINODEOWNERSMSGEX_H
#define FIXINODEOWNERSMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/FixInodeOwnersMsg.h>
#include <common/net/message/fsck/FixInodeOwnersRespMsg.h>

class FixInodeOwnersMsgEx : public FixInodeOwnersMsg
{
   public:
      FixInodeOwnersMsgEx() : FixInodeOwnersMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
         size_t bufLen, HighResolutionStats* stats);

   private:

};

#endif /*FIXINODEOWNERSMSGEX_H*/
