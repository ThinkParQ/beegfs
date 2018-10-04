#include <common/net/message/nodes/SetMirrorBuddyGroupRespMsg.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/toolkit/MessagingTk.h>

#include <program/Program.h>

#include "SetMirrorBuddyGroupMsgEx.h"

bool SetMirrorBuddyGroupMsgEx::processIncoming(ResponseContext& ctx)
{
   uint16_t buddyGroupID = this->getBuddyGroupID();

   if (getNodeType() != NODETYPE_Storage)
   {
      // The storage server has no mapper for meta buddy groups - nothing to do, just acknowledge
      if (!acknowledge(ctx))
         ctx.sendResponse(SetMirrorBuddyGroupRespMsg(FhgfsOpsErr_SUCCESS, buddyGroupID));
      return true;
   }

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();

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

