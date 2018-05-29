#ifndef CHANGETARGETCONSISTENCYSTATESMSG_H
#define CHANGETARGETCONSISTENCYSTATESMSG_H

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/Node.h>

/*
 *  NOTE: this message will be used in mgmtd to update target state store, but also in storage
 *  server to set local target states
 */

class ChangeTargetConsistencyStatesMsg
   : public AcknowledgeableMsgSerdes<ChangeTargetConsistencyStatesMsg>
{
   public:
      ChangeTargetConsistencyStatesMsg(NodeType nodeType, UInt16List* targetIDs,
         UInt8List* oldStates, UInt8List* newStates)
         : BaseType(NETMSGTYPE_ChangeTargetConsistencyStates),
            nodeType(nodeType),
            targetIDs(targetIDs),
            oldStates(oldStates),
            newStates(newStates)
      {
      }

      ChangeTargetConsistencyStatesMsg()
         : BaseType(NETMSGTYPE_ChangeTargetConsistencyStates)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeType
            % serdes::backedPtr(obj->targetIDs, obj->parsed.targetIDs)
            % serdes::backedPtr(obj->oldStates, obj->parsed.oldStates)
            % serdes::backedPtr(obj->newStates, obj->parsed.newStates);

         obj->serializeAckID(ctx, 4);
      }

   private:
      int32_t nodeType;

      UInt16List* targetIDs; // not owned by this object!
      UInt8List* oldStates; // not owned by this object!
      UInt8List* newStates; // not owned by this object!

      // for deserialization
      struct {
         UInt16List targetIDs;
         UInt8List oldStates;
         UInt8List newStates;
      } parsed;

   public:
      NodeType getNodeType()
      {
         return (NodeType)nodeType;
      }

      UInt16List& getTargetIDs()
      {
         return *targetIDs;
      }

      UInt8List& getOldStates()
      {
         return *oldStates;
      }

      UInt8List& getNewStates()
      {
         return *newStates;
      }
};

#endif /*CHANGETARGETCONSISTENCYSTATESMSG_H*/
