#ifndef COMMON_NET_MESSAGE_CONTROL_PEERINFOMSG_H
#define COMMON_NET_MESSAGE_CONTROL_PEERINFOMSG_H

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>

struct PeerInfoMsg;
typedef struct PeerInfoMsg PeerInfoMsg;

struct PeerInfoMsg
{
   NetMessage netMessage;

   NodeType type;
   NumNodeID id;
};

extern const struct NetMessageOps PeerInfoMsg_Ops;

static inline void PeerInfoMsg_init(PeerInfoMsg* this, NodeType type, NumNodeID id)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_PeerInfo, &PeerInfoMsg_Ops);

   this->type = type;
   this->id = id;
}

#endif
