#ifndef HEARTBEATMSGEX_H_
#define HEARTBEATMSGEX_H_

#include <common/net/message/nodes/HeartbeatMsg.h>

class HeartbeatMsgEx : public HeartbeatMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      void processIncomingRoot();
};

#endif /*HEARTBEATMSGEX_H_*/
