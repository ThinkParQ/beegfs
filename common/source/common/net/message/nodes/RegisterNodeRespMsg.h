#pragma once

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <stdint.h>

class RegisterNodeRespMsg : public NetMessageSerdes<RegisterNodeRespMsg>
{
   public:
      /**
       * @param nodeNumID 0 on error (e.g. if given nodeNumID from RegisterNodeMsg was rejected),
       * newly assigned numeric ID otherwise (or the old numeric ID value if it was given in
       * RegisterNodeMsg and was accepted).
       */
      RegisterNodeRespMsg(NumNodeID nodeNumID) :
         BaseType(NETMSGTYPE_RegisterNodeResp), nodeNumID(nodeNumID)
      {
      }

      /**
       * For deserialization only
       */
      RegisterNodeRespMsg() : BaseType(NETMSGTYPE_RegisterNodeResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->nodeNumID;
         ctx % obj->grpcPort;
         ctx % obj->fsUUID;
      }

   private:
      NumNodeID nodeNumID;
      // grpcPort and fsUUID are currently only used on the client. We deserialize them here for
      // completeness, but don't implement any getters for now.
      uint16_t grpcPort;
      std::string fsUUID;

   public:
      // getters & setters
      NumNodeID getNodeNumID() const
      {
         return nodeNumID;
      }
};


