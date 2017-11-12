#ifndef SETMIRRORBUDDYGROUPMSG_H_
#define SETMIRRORBUDDYGROUPMSG_H_

#include <common/Common.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/Node.h>

class SetMirrorBuddyGroupMsg : public AcknowledgeableMsgSerdes<SetMirrorBuddyGroupMsg>
{
   public:
      /**
       * Create or update a mirror buddy group
       *
       * @param primaryTargetID
       * @param secondaryTargetID
       * @param buddyGroupID may be 0 => create a buddy group with random ID
       * @param allowUpdate if buddyGroupID is set and that group exists, allow to update it
       */
      SetMirrorBuddyGroupMsg(NodeType nodeType, uint16_t primaryTargetID,
         uint16_t secondaryTargetID, uint16_t buddyGroupID = 0, bool allowUpdate = false)
         : BaseType(NETMSGTYPE_SetMirrorBuddyGroup),
           nodeType(nodeType),
           primaryTargetID(primaryTargetID),
           secondaryTargetID(secondaryTargetID),
           buddyGroupID(buddyGroupID),
           allowUpdate(allowUpdate)
      { }

      SetMirrorBuddyGroupMsg() : BaseType(NETMSGTYPE_SetMirrorBuddyGroup)
      { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeType
            % obj->primaryTargetID
            % obj->secondaryTargetID
            % obj->buddyGroupID
            % obj->allowUpdate;

         obj->serializeAckID(ctx);
      }

   private:
      int32_t nodeType;
      uint16_t primaryTargetID;
      uint16_t secondaryTargetID;
      uint16_t buddyGroupID;
      bool allowUpdate;

   public:
      // getters & setters
      NodeType getNodeType() const
      {
         return (NodeType)nodeType;
      }

      uint16_t getPrimaryTargetID() const
      {
         return primaryTargetID;
      }

      uint16_t getSecondaryTargetID() const
      {
         return secondaryTargetID;
      }

      uint16_t getBuddyGroupID() const
      {
         return buddyGroupID;
      }

      bool getAllowUpdate() const
      {
         return allowUpdate;
      }
};

#endif /* SETMIRRORBUDDYGROUPMSG_H_ */
