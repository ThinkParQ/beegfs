#include "RemoveBuddyGroupMsgEx.h"

#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/RemoveBuddyGroupRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

static FhgfsOpsErr removeGroupOnRemoteNode(Node& node, const uint16_t groupID, const bool onlyCheck,
      const bool force)
{
   RemoveBuddyGroupMsg msg(NODETYPE_Storage, groupID, onlyCheck, force);

   const auto resultMsg = MessagingTk::requestResponse(node, msg, NETMSGTYPE_RemoveBuddyGroupResp);
   if (!resultMsg)
      return FhgfsOpsErr_COMMUNICATION;

   return static_cast<RemoveBuddyGroupRespMsg&>(*resultMsg).getResult();
}

bool RemoveBuddyGroupMsgEx::processIncoming(ResponseContext& ctx)
{
   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   if (type != NODETYPE_Storage)
   {
      LOG(MIRRORING, ERR, "Attempted to remove non-storage buddy group.", groupID, type);
      ctx.sendResponse(RemoveBuddyGroupRespMsg(FhgfsOpsErr_INTERNAL));
      return true;
   }

   auto* const nodes = Program::getApp()->getStorageNodes();
   auto* const targetMapper = Program::getApp()->getTargetMapper();
   auto* const bgm = Program::getApp()->getStorageBuddyGroupMapper();

   const MirrorBuddyGroup group = bgm->getMirrorBuddyGroup(groupID);

   if (group.firstTargetID == 0)
   {
      LOG(MIRRORING, ERR, "Attempted to remove unknown storage buddy group.", groupID);
      ctx.sendResponse(RemoveBuddyGroupRespMsg(FhgfsOpsErr_UNKNOWNTARGET));
      return true;
   }

   const auto primary = nodes->referenceNode(targetMapper->getNodeID(group.firstTargetID));
   const auto secondary = nodes->referenceNode(targetMapper->getNodeID(group.secondTargetID));

   if (!primary || !secondary)
   {
      if (!primary)
         LOG(MIRRORING, ERR, "Could not reference primary of group.", groupID);
      if (!secondary)
         LOG(MIRRORING, ERR, "Could not reference secondary of group.", groupID);

      ctx.sendResponse(RemoveBuddyGroupRespMsg(FhgfsOpsErr_UNKNOWNTARGET));
      return true;
   }

   const FhgfsOpsErr checkPrimaryRes = removeGroupOnRemoteNode(*primary, groupID, true, force);
   if (checkPrimaryRes != FhgfsOpsErr_SUCCESS)
   {
      LOG(MIRRORING, NOTICE, "Could not remove storage buddy group, targets are still in use.", groupID);
      ctx.sendResponse(RemoveBuddyGroupRespMsg(checkPrimaryRes));
      return true;
   }

   const FhgfsOpsErr checkSecondaryRes = removeGroupOnRemoteNode(*secondary, groupID, true, force);
   if (checkSecondaryRes != FhgfsOpsErr_SUCCESS)
   {
      LOG(MIRRORING, NOTICE, "Could not remove storage buddy group, targets are still in use.", groupID);
      ctx.sendResponse(RemoveBuddyGroupRespMsg(checkSecondaryRes));
      return true;
   }

   if (!checkOnly)
   {
      const bool removeRes = bgm->unmapMirrorBuddyGroup(groupID, NumNodeID());
      if (!removeRes)
      {
         LOG(MIRRORING, ERR, "Could not unmap storage buddy group.", groupID);
         ctx.sendResponse(RemoveBuddyGroupRespMsg(FhgfsOpsErr_INTERNAL));
         return true;
      }

      // we don't care much about the results of group removal on the storage servers. if we fail
      // here, it is possible that the internode syncer has already removed the groups on remote
      // nodes.
      removeGroupOnRemoteNode(*primary, groupID, false, force);
      removeGroupOnRemoteNode(*secondary, groupID, false, force);
   }

   ctx.sendResponse(RemoveBuddyGroupRespMsg(FhgfsOpsErr_SUCCESS));
   return true;
}
