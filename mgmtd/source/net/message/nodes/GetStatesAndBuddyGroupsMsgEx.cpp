#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "GetStatesAndBuddyGroupsMsgEx.h"

bool GetStatesAndBuddyGroupsMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(STATES, DEBUG, "Received a GetStatesAndBuddyGroupsMsg.", ctx.peerName() );

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
         LOG(STATES, ERR, "Invalid node type.",
               as("Node Type", Node::nodeTypeToStr(nodeType)),
               as("Sender", ctx.peerName())
            );
         return false;
   }

   UInt16List targetIDs;
   UInt8List targetReachabilityStates;
   UInt8List targetConsistencyStates;

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;

   targetStateStore->getStatesAndGroupsAsLists(buddyGroupMapper,
      targetIDs, targetReachabilityStates, targetConsistencyStates,
      buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs);

   ctx.sendResponse(
         GetStatesAndBuddyGroupsRespMsg(&buddyGroupIDs, &primaryTargetIDs,
            &secondaryTargetIDs, &targetIDs, &targetReachabilityStates, &targetConsistencyStates) );

   return true;
}

