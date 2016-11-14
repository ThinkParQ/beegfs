#ifndef COMMON_NET_MESSAGE_CONTROL_PEERINFOMSG_H
#define COMMON_NET_MESSAGE_CONTROL_PEERINFOMSG_H

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>

class PeerInfoMsg : public NetMessageSerdes<PeerInfoMsg>
{
   public:
      PeerInfoMsg(NodeType type, NumNodeID id)
         : BaseType(NETMSGTYPE_PeerInfo), type(type), id(id)
      {
      }

      /**
       * For deserialization only
       */
      PeerInfoMsg() : BaseType(NETMSGTYPE_PeerInfo)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::as<uint32_t>(obj->type)
            % obj->id;
      }

   protected:
      NodeType type;
      NumNodeID id;
};

#endif /* AUTHENTICATECHANNELMSG_H_ */
