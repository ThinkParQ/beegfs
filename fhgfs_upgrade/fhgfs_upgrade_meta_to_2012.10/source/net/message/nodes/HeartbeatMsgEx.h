#ifndef HEARTBEATMSGEX_H_
#define HEARTBEATMSGEX_H_

#include <common/net/message/nodes/HeartbeatMsg.h>

class HeartbeatMsgEx : public HeartbeatMsg
{
   public:
      HeartbeatMsgEx() : HeartbeatMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:
      
   private:
      void processIncomingRoot();
};

#endif /*HEARTBEATMSGEX_H_*/
