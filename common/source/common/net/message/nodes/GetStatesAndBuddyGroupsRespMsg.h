#ifndef GETSTATESANDBUDDYGROUPSRESPMSG_H_
#define GETSTATESANDBUDDYGROUPSRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/MirrorBuddyGroup.h>
#include <common/nodes/TargetStateInfo.h>


/**
 * This message carries two maps:
 *    1) buddyGroupID -> primaryTarget, secondaryTarget
 *    2) targetID -> targetReachabilityState, targetConsistencyState
 */
class GetStatesAndBuddyGroupsRespMsg : public NetMessageSerdes<GetStatesAndBuddyGroupsRespMsg>
{
   public:
      GetStatesAndBuddyGroupsRespMsg(const MirrorBuddyGroupMap& groups,
            const TargetStateMap& states) :
            BaseType(NETMSGTYPE_GetStatesAndBuddyGroupsResp),
            groups(&groups), states(&states)
      {
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
            % serdes::backedPtr(obj->groups, obj->parsed.groups)
            % serdes::backedPtr(obj->states, obj->parsed.states);
      }

   private:
      const MirrorBuddyGroupMap* groups;
      const TargetStateMap* states;

      // for deserialization
      struct {
         MirrorBuddyGroupMap groups;
         TargetStateMap states;
      } parsed;

   public:
      const MirrorBuddyGroupMap& getGroups() const { return *groups; }

      MirrorBuddyGroupMap releaseGroups()
      {
         return groups == &parsed.groups ? std::move(parsed.groups) : *groups;
      }

      const TargetStateMap& getStates() const { return *states; }

      TargetStateMap releaseStates()
      {
         return states == &parsed.states ? std::move(parsed.states) : *states;
      }
};

#endif /* GETSTATESANDBUDDYGROUPSRESPMSG_H_ */
