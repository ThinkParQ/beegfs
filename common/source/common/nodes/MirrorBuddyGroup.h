#ifndef COMMON_MIRRORBUDDYGROUP_H_
#define COMMON_MIRRORBUDDYGROUP_H_

#include <common/Common.h>

struct MirrorBuddyGroup
{
   uint16_t firstTargetID = 0;
   uint16_t secondTargetID = 0;

   MirrorBuddyGroup() = default;

   MirrorBuddyGroup(uint16_t firstTargetID, uint16_t secondTargetID):
      firstTargetID(firstTargetID), secondTargetID(secondTargetID)
   {
   }

   template<typename This, typename Ctx>
   static void serialize(This* obj, Ctx& ctx)
   {
      ctx
         % obj->firstTargetID
         % obj->secondTargetID;
   }
};


typedef std::list<MirrorBuddyGroup> MirrorBuddyGroupList;
typedef MirrorBuddyGroupList::iterator MirrorBuddyGroupListIter;
typedef MirrorBuddyGroupList::const_iterator MirrorBuddyGroupListCIter;

typedef std::map<uint16_t, MirrorBuddyGroup> MirrorBuddyGroupMap; // keys: MBG-IDs, values: MBGs
typedef MirrorBuddyGroupMap::iterator MirrorBuddyGroupMapIter;
typedef MirrorBuddyGroupMap::const_iterator MirrorBuddyGroupMapCIter;
typedef MirrorBuddyGroupMap::value_type MirrorBuddyGroupMapVal;

#endif
