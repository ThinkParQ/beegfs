#ifndef MAPTARGETSMSG_H_
#define MAPTARGETSMSG_H_

#include <common/Common.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/StoragePoolId.h>

class MapTargetsMsg : public AcknowledgeableMsgSerdes<MapTargetsMsg>
{
   public:
       /**
       * Maps all targetIDs to the same given nodeID.
       *
       * @param targets each target has a corresponding storage pool id to map it to (pool id might
       *        be ignored on the receiving side, if the target is not newly mapped).
       *        note: just a reference => do not free while you're using this object
       * @param nodeID numeric node ID
       */
      MapTargetsMsg(const std::map<uint16_t, StoragePoolId>& targets, NumNodeID nodeID):
         BaseType(NETMSGTYPE_MapTargets),
         nodeID(nodeID), targets(&targets)
      {
      }

      MapTargetsMsg() : BaseType(NETMSGTYPE_MapTargets)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->targets, obj->parsed.targets)
            % obj->nodeID;

         obj->serializeAckID(ctx);
      }

      NumNodeID getNodeID() const
      {
         return nodeID;
      }

      const std::map<uint16_t, StoragePoolId>& getTargets() const
      {
         return *targets;
      }

   private:
      NumNodeID nodeID;

      // for serialization
      const std::map<uint16_t, StoragePoolId>* targets; // not owned by this object!

      // for deserialization
      struct {
         std::map<uint16_t, StoragePoolId> targets;
      } parsed;
};

#endif /* MAPTARGETSMSG_H_ */
