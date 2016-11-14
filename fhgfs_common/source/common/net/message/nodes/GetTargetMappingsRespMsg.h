#ifndef GETTARGETMAPPINGSRESPMSG_H_
#define GETTARGETMAPPINGSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>

class GetTargetMappingsRespMsg : public NetMessageSerdes<GetTargetMappingsRespMsg>
{
   public:
      /**
       * @param targetIDs just a reference => do not free while you're using this object
       * @param nodeIDs just a reference => do not free while you're using this object
       */
      GetTargetMappingsRespMsg(UInt16List* targetIDs, NumNodeIDList* nodeIDs) :
         BaseType(NETMSGTYPE_GetTargetMappingsResp)
      {
         this->targetIDs = targetIDs;
         this->nodeIDs = nodeIDs;
      }

      GetTargetMappingsRespMsg() : BaseType(NETMSGTYPE_GetTargetMappingsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->targetIDs, obj->parsed.targetIDs)
            % serdes::backedPtr(obj->nodeIDs, obj->parsed.nodeIDs);
      }

   private:
      // for serialization
      UInt16List* targetIDs; // not owned by this object!
      NumNodeIDList* nodeIDs; // not owned by this object!

      // for deserialization
      struct {
         UInt16List targetIDs;
         NumNodeIDList nodeIDs;
      } parsed;


   public:
      UInt16List& getTargetIDs()
      {
         return *targetIDs;
      }

      NumNodeIDList& getNodeIDs()
      {
         return *nodeIDs;
      }

};

#endif /* GETTARGETMAPPINGSRESPMSG_H_ */
