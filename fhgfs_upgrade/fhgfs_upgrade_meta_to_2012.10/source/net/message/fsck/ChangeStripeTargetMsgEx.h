#ifndef CHANGESTRIPETARGETMSGEX_H
#define CHANGESTRIPETARGETMSGEX_H

#include <common/net/message/fsck/ChangeStripeTargetMsg.h>
#include <common/net/message/fsck/ChangeStripeTargetRespMsg.h>

class ChangeStripeTargetMsgEx : public ChangeStripeTargetMsg
{
   public:
      ChangeStripeTargetMsgEx() : ChangeStripeTargetMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
               char* respBuf, size_t bufLen, HighResolutionStats* stats);

   private:
      void changePattern(StripePattern* pattern);
};

#endif /*CHANGESTRIPETARGETMSGEX_H*/
