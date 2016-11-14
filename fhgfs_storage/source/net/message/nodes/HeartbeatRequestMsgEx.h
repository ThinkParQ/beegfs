#ifndef HEARTBEATREQUESTMSGEX_H_
#define HEARTBEATREQUESTMSGEX_H_

#include <common/net/message/nodes/HeartbeatRequestMsg.h>

class HeartbeatRequestMsgEx : public HeartbeatRequestMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*HEARTBEATREQUESTMSGEX_H_*/
