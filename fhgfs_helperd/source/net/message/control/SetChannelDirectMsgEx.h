#ifndef SETCHANNELDIRECTMSGEX_H_
#define SETCHANNELDIRECTMSGEX_H_

#include <common/net/message/control/SetChannelDirectMsg.h>

class SetChannelDirectMsgEx : public SetChannelDirectMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /*SETCHANNELDIRECTMSGEX_H_*/
