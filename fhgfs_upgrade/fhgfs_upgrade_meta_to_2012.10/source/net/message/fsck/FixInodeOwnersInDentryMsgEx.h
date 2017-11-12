#ifndef FIXINODEOWNERSINDENTRYMSGEX_H
#define FIXINODEOWNERSINDENTRYMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/FixInodeOwnersInDentryMsg.h>
#include <common/net/message/fsck/FixInodeOwnersInDentryRespMsg.h>

class FixInodeOwnersInDentryMsgEx : public FixInodeOwnersInDentryMsg
{
   public:
      FixInodeOwnersInDentryMsgEx() : FixInodeOwnersInDentryMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
         size_t bufLen, HighResolutionStats* stats);

   private:

};

#endif /*FIXINODEOWNERSINDENTRYMSGEX_H*/
