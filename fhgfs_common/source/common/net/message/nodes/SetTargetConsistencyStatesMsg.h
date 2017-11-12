#ifndef SETTARGETCONSISTENCYSTATESMSG_H
#define SETTARGETCONSISTENCYSTATESMSG_H

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/Node.h>
#include <common/Common.h>

/*
 *  NOTE: this message will be used in mgmtd to update target state store, but also in storage
 *  server to set local target states
 */

class SetTargetConsistencyStatesMsg : public AcknowledgeableMsgSerdes<SetTargetConsistencyStatesMsg>
{
   public:
      SetTargetConsistencyStatesMsg(NodeType nodeType, UInt16List* targetIDs, UInt8List* states,
         bool setOnline) :
         BaseType(NETMSGTYPE_SetTargetConsistencyStates)
      {
         this->targetIDs = targetIDs;
         this->states = states;
         this->setOnline = setOnline;
         this->nodeType = nodeType;
      }

      SetTargetConsistencyStatesMsg() : BaseType(NETMSGTYPE_SetTargetConsistencyStates)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeType
            % serdes::backedPtr(obj->targetIDs, obj->parsed.targetIDs)
            % serdes::backedPtr(obj->states, obj->parsed.states);

         obj->serializeAckID(ctx, 4);

         ctx % obj->setOnline;
      }

   private:
      int nodeType;

      UInt16List* targetIDs; // not owned by this object!
      UInt8List* states; // not owned by this object!

      /**
       * setOnline shall be set to "true" when reporting states for local targets (i.e. targets
       *           which are on the reporting server). When receiving state reports for buddy
       *           targets (the primary telling the mgmtd that the secondary needs a resync), the
       *           targets should not be ONLINEd again.
       */
      bool setOnline;


      // for deserialization
      struct {
         UInt16List targetIDs;
         UInt8List states;
      } parsed;

   public:
      NodeType getNodeType()
      {
         return static_cast<NodeType>(nodeType);
      }

      UInt16List& getTargetIDs()
      {
         return *targetIDs;
      }

      UInt8List& getStates()
      {
         return *states;
      }

      bool getSetOnline()
      {
         return this->setOnline;
      }
};

#endif /*SETTARGETCONSISTENCYSTATESMSG_H*/
