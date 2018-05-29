#ifndef GETSTATESANDBUDDYGROUPSRESPMSG_H_
#define GETSTATESANDBUDDYGROUPSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>


/**
 * This message carries two maps:
 *    1) buddyGroupID -> primaryTarget, secondaryTarget
 *    2) targetID -> targetReachabilityState, targetConsistencyState
 */
class GetStatesAndBuddyGroupsRespMsg : public NetMessageSerdes<GetStatesAndBuddyGroupsRespMsg>
{
   public:
      GetStatesAndBuddyGroupsRespMsg(UInt16List* buddyGroupIDs, UInt16List* primaryTargetIDs,
            UInt16List* secondaryTargetIDs, UInt16List* targetIDs,
            UInt8List* targetReachabilityStates, UInt8List* targetConsistencyStates) :
            BaseType(NETMSGTYPE_GetStatesAndBuddyGroupsResp)
      {
         this->buddyGroupIDs = buddyGroupIDs;
         this->primaryTargetIDs = primaryTargetIDs;
         this->secondaryTargetIDs = secondaryTargetIDs;

         this->targetIDs = targetIDs;
         this->targetReachabilityStates = targetReachabilityStates;
         this->targetConsistencyStates = targetConsistencyStates;
      }

      /**
       * For deserialization only.
       */
      GetStatesAndBuddyGroupsRespMsg() :
            BaseType(NETMSGTYPE_GetStatesAndBuddyGroupsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->buddyGroupIDs, obj->parsed.buddyGroupIDs)
            % serdes::backedPtr(obj->primaryTargetIDs, obj->parsed.primaryTargetIDs)
            % serdes::backedPtr(obj->secondaryTargetIDs, obj->parsed.secondaryTargetIDs)
            % serdes::backedPtr(obj->targetIDs, obj->parsed.targetIDs)
            % serdes::backedPtr(obj->targetReachabilityStates, obj->parsed.targetReachabilityStates)
            % serdes::backedPtr(obj->targetConsistencyStates, obj->parsed.targetConsistencyStates);
      }

   private:
      UInt16List* buddyGroupIDs; // Not owned by this object!
      UInt16List* primaryTargetIDs; // Not owned by this object!
      UInt16List* secondaryTargetIDs; // Not owned by this object!

      UInt16List* targetIDs; // Not owned by this object!
      UInt8List* targetReachabilityStates; // Not owned by this object!
      UInt8List* targetConsistencyStates; // Not owned by this object!

      // for deserialization
      struct {
         UInt16List buddyGroupIDs;
         UInt16List primaryTargetIDs;
         UInt16List secondaryTargetIDs;
         UInt16List targetIDs;
         UInt8List targetReachabilityStates;
         UInt8List targetConsistencyStates;
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

      UInt16List& getTargetIDs()
      {
         return *targetIDs;
      }

      UInt8List& getReachabilityStates()
      {
         return *targetReachabilityStates;
      }

      UInt8List& getConsistencyStates()
      {
         return *targetConsistencyStates;
      }
};

#endif /* GETSTATESANDBUDDYGROUPSRESPMSG_H_ */
