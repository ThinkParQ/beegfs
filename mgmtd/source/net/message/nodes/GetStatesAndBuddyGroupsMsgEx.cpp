#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "GetStatesAndBuddyGroupsMsgEx.h"

bool GetStatesAndBuddyGroupsMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   TargetStateStore* targetStateStore;
   MirrorBuddyGroupMapper* buddyGroupMapper;
   NodeType nodeType = getNodeType();

   switch (nodeType)
   {
      case NODETYPE_Storage:
         buddyGroupMapper = app->getStorageBuddyGroupMapper();
         targetStateStore = app->getTargetStateStore();
         break;

      case NODETYPE_Meta:
         buddyGroupMapper = app->getMetaBuddyGroupMapper();
         targetStateStore = app->getMetaStateStore();
         break;

      default:
         LOG(STATES, ERR, "Invalid node type.", nodeType, ("Sender", ctx.peerName()));
         return false;
   }

   MirrorBuddyGroupMap groups;
   TargetStateMap states;

   targetStateStore->getStatesAndGroups(buddyGroupMapper, states, groups);

   ctx.sendResponse(GetStatesAndBuddyGroupsRespMsg(groups, states));

   return true;
}

