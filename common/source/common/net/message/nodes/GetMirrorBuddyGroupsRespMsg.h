#ifndef GETMIRRORBUDDYGROUPSRESPMSG_H_
#define GETMIRRORBUDDYGROUPSRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>


class GetMirrorBuddyGroupsRespMsg : public NetMessageSerdes<GetMirrorBuddyGroupsRespMsg>
{
   public:
      GetMirrorBuddyGroupsRespMsg(UInt16List* buddyGroupIDs, UInt16List* primaryTargetIDs,
         UInt16List* secondaryTargetIDs) : BaseType(NETMSGTYPE_GetMirrorBuddyGroupsResp)
      {
         this->buddyGroupIDs = buddyGroupIDs;
         this->primaryTargetIDs = primaryTargetIDs;
         this->secondaryTargetIDs = secondaryTargetIDs;
      }

      GetMirrorBuddyGroupsRespMsg() :
         BaseType(NETMSGTYPE_GetMirrorBuddyGroupsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->buddyGroupIDs, obj->parsed.buddyGroupIDs)
            % serdes::backedPtr(obj->primaryTargetIDs, obj->parsed.primaryTargetIDs)
            % serdes::backedPtr(obj->secondaryTargetIDs, obj->parsed.secondaryTargetIDs);
      }

   private:
      UInt16List* buddyGroupIDs; // not owned by this object!
      UInt16List* primaryTargetIDs; // not owned by this object!
      UInt16List* secondaryTargetIDs; // not owned by this object!

      // for deserialization
      struct {
         UInt16List buddyGroupIDs;
         UInt16List primaryTargetIDs;
         UInt16List secondaryTargetIDs;
      } parsed;

   public:
      UInt16List& getBuddyGroupIDs()
      {
         return *buddyGroupIDs;
      }

      UInt16List& getPrimaryTargetIDs()
      {
         return *primaryTargetIDs;
      }

      UInt16List& getSecondaryTargetIDs()
      {
         return *secondaryTargetIDs;
      }
};


#endif /* GETMIRRORBUDDYGROUPSRESPMSG_H_ */
