#ifndef COMMON_NET_MESSAGE_CONTROL_PEERINFOMSGEX_H
#define COMMON_NET_MESSAGE_CONTROL_PEERINFOMSGEX_H

#include <common/net/message/control/PeerInfoMsg.h>

class PeerInfoMsgEx : public PeerInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* AUTHENTICATECHANNELMSGEX_H_ */
