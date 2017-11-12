#include <common/net/message/nodes/GetMirrorBuddyGroupsRespMsg.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <program/Program.h>
#include "GetMirrorBuddyGroupsMsgEx.h"


bool GetMirrorBuddyGroupsMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(STATES, DEBUG, "Received a GetMirrorBuddyGroupsMsg." + ctx.peerName() );

   App* app = Program::getApp();

   MirrorBuddyGroupMapper* buddyGroupMapper;

   NodeType nodeType = getNodeType();

   switch (nodeType)
   {
      case NODETYPE_Storage:
         buddyGroupMapper = app->getStorageBuddyGroupMapper();
         break;

      case NODETYPE_Meta:
         buddyGroupMapper = app->getMetaBuddyGroupMapper();
         break;

      default:
         LOG(MIRRORING, ERR, "Invalid node type", nodeType);
         return false;
   }

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;

   buddyGroupMapper->getMappingAsLists(buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs);

   ctx.sendResponse(
         GetMirrorBuddyGroupsRespMsg(&buddyGroupIDs, &primaryTargetIDs, &secondaryTargetIDs) );

   return true;
}

