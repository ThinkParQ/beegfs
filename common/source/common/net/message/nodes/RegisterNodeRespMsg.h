#ifndef REGISTERNODERESPMSG_H_
#define REGISTERNODERESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>

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
      }

   private:
      NumNodeID nodeNumID;

   public:
      // getters & setters
      NumNodeID getNodeNumID() const
      {
         return nodeNumID;
      }
};


#endif /* REGISTERNODERESPMSG_H_ */
