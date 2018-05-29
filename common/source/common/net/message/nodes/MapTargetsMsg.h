#ifndef MAPTARGETSMSG_H_
#define MAPTARGETSMSG_H_

#include <common/Common.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/StoragePoolId.h>

typedef std::vector<std::pair<uint16_t, StoragePoolId>> TargetPoolPairVec;

class MapTargetsMsg : public AcknowledgeableMsgSerdes<MapTargetsMsg>
{
   public:
       /**
       * Maps all targetIDs to the same given nodeID.
       *
       * @param targetVec the targetVec is actually a vector of pair<uint16_t, StoragePoolId>,
       *        i.e. each target has a corresponding storage pool id to map it to (pool id might
       *        be ignored on the receiving side, if the target is not newly mapped).
       *        note: just a reference => do not free while you're using this object
       * @param nodeID numeric node ID
       */
      MapTargetsMsg(TargetPoolPairVec* targetVec, NumNodeID nodeID):
            BaseType(NETMSGTYPE_MapTargets), nodeID(nodeID), targetVec(targetVec)
      {
      }

      MapTargetsMsg() : BaseType(NETMSGTYPE_MapTargets)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->targetVec, obj->parsed.targetVec)
            % obj->nodeID;

         obj->serializeAckID(ctx);
      }

      NumNodeID getNodeID() const
      {
         return nodeID;
      }

      const TargetPoolPairVec& getTargetVec() const
      {
         return *targetVec;
      }

   private:
      NumNodeID nodeID;

      // for serialization
      TargetPoolPairVec* targetVec; // not owned by this object!

      // for deserialization
      struct {
         TargetPoolPairVec targetVec;
      } parsed;
};

#endif /* MAPTARGETSMSG_H_ */
