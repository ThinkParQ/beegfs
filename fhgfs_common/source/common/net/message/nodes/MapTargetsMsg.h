#ifndef MAPTARGETSMSG_H_
#define MAPTARGETSMSG_H_

#include <common/Common.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/NumNodeID.h>

class MapTargetsMsg : public AcknowledgeableMsgSerdes<MapTargetsMsg>
{
   public:
      /**
       * Maps all targetIDs to the same given nodeID.
       *
       * @param targetIDs just a reference => do not free while you're using this object
       * @param nodeID just a reference => do not free while you're using this object
       */
      MapTargetsMsg(UInt16List* targetIDs, NumNodeID nodeID) :
         BaseType(NETMSGTYPE_MapTargets)
      {
         this->targetIDs = targetIDs;

         this->nodeID = nodeID;
      }

      MapTargetsMsg() : BaseType(NETMSGTYPE_MapTargets)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->targetIDs, obj->parsed.targetIDs)
            % obj->nodeID;

         obj->serializeAckID(ctx);
      }

   private:
      NumNodeID nodeID;

      // for serialization
      UInt16List* targetIDs; // not owned by this object!

      // for deserialization
      struct {
         UInt16List targetIDs;
      } parsed;


   public:
      const UInt16List& getTargetIDs()
      {
         return *targetIDs;
      }

      NumNodeID getNodeID() const
      {
         return nodeID;
      }
};

#endif /* MAPTARGETSMSG_H_ */
