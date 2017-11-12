#ifndef REMOVENODEMSGEX_H_
#define REMOVENODEMSGEX_H_

#include <common/net/message/nodes/RemoveNodeMsg.h>

class RemoveNodeMsgEx : public RemoveNodeMsg
{
   public:
      RemoveNodeMsgEx() : RemoveNodeMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
         
   protected:
};


#endif /* REMOVENODEMSGEX_H_ */
