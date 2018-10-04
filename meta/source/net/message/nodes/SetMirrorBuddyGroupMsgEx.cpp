#include <common/net/message/nodes/SetMirrorBuddyGroupRespMsg.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/toolkit/MessagingTk.h>

#include <program/Program.h>

#include "SetMirrorBuddyGroupMsgEx.h"

bool SetMirrorBuddyGroupMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("SetMirrorBuddyGroupMsg incoming");

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
         log.logErr("Node type mismatch");
         return false;
   }


   uint16_t buddyGroupID = this->getBuddyGroupID();
   uint16_t primaryTargetID = this->getPrimaryTargetID();
   uint16_t secondaryTargetID = this->getSecondaryTargetID();
   bool allowUpdate = this->getAllowUpdate();
   uint16_t newBuddyGroupID = 0;

   FhgfsOpsErr mapResult = buddyGroupMapper->mapMirrorBuddyGroup(buddyGroupID, primaryTargetID,
      secondaryTargetID, app->getLocalNode().getNumID(), allowUpdate, &newBuddyGroupID);

   if(!acknowledge(ctx) )
      ctx.sendResponse(SetMirrorBuddyGroupRespMsg(mapResult, newBuddyGroupID) );

   return true;
}

