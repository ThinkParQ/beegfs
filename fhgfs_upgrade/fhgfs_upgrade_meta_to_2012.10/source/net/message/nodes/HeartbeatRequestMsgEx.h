#ifndef HEARTBEATREQUESTMSGEX_H_
#define HEARTBEATREQUESTMSGEX_H_

#include <common/net/message/nodes/HeartbeatRequestMsg.h>

class HeartbeatRequestMsgEx : public HeartbeatRequestMsg
{
   public:
      HeartbeatRequestMsgEx() : HeartbeatRequestMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
         
   protected:
};

#endif /*HEARTBEATREQUESTMSGEX_H_*/
