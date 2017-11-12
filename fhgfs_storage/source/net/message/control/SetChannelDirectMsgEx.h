#ifndef SETCHANNELDIRECTMSGEX_H_
#define SETCHANNELDIRECTMSGEX_H_

#include <common/net/message/control/SetChannelDirectMsg.h>

// direct means the message is definitely processed on this server and not forwarded to another

class SetChannelDirectMsgEx : public SetChannelDirectMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /*SETCHANNELDIRECTMSGEX_H_*/
