#ifndef AUTHENTICATECHANNELMSGEX_H_
#define AUTHENTICATECHANNELMSGEX_H_

#include <common/net/message/control/AuthenticateChannelMsg.h>


class AuthenticateChannelMsgEx : public AuthenticateChannelMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* AUTHENTICATECHANNELMSGEX_H_ */
