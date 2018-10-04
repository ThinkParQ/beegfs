#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/SetMirrorBuddyGroupRespMsg.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/toolkit/MessagingTk.h>

#include <program/Program.h>

#include "SetMirrorBuddyGroupMsgEx.h"


bool SetMirrorBuddyGroupMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   InternodeSyncer* internodeSyncer = app->getInternodeSyncer();
   MirrorBuddyGroupMapper* buddyGroupMapper;

   uint16_t buddyGroupID = this->getBuddyGroupID();
   uint16_t primaryTargetID = this->getPrimaryTargetID();
   uint16_t secondaryTargetID = this->getSecondaryTargetID();
   bool allowUpdate = this->getAllowUpdate();
   uint16_t newBuddyGroupID = 0;

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
         LOG(MIRRORING, ERR, "Node type mismatch.", nodeType);
         return false;
   }

   FhgfsOpsErr mapResult = buddyGroupMapper->mapMirrorBuddyGroup(buddyGroupID, primaryTargetID,
      secondaryTargetID, app->getLocalNode().getNumID(), allowUpdate, &newBuddyGroupID);

   if (mapResult == FhgfsOpsErr_SUCCESS)
   {
      app->getHeartbeatMgr()->notifyAsyncAddedMirrorBuddyGroup(nodeType, newBuddyGroupID,
         primaryTargetID, secondaryTargetID);
      app->getHeartbeatMgr()->notifyAsyncRefreshCapacityPools();
   }

   // Force pools update, so that newly created group is put into correct capacity pool.
   internodeSyncer->setForcePoolsUpdate();

   ctx.sendResponse(SetMirrorBuddyGroupRespMsg(mapResult, newBuddyGroupID) );

   return true;
}

