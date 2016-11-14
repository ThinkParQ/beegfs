#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "GetStatesAndBuddyGroupsMsgEx.h"

bool GetStatesAndBuddyGroupsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetStatesAndBuddyGroupsMsg incoming";

   LOG_DEBUG(logContext, Log_DEBUG, "Received a GetStatesAndBuddyGroupsMsg from: "
      + ctx.peerName() );

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
         LogContext(logContext).logErr("Invalid node type: " + StringTk::intToStr(nodeType) );
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

