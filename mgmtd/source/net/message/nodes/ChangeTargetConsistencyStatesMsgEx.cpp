#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/ChangeTargetConsistencyStatesRespMsg.h>
#include <common/nodes/TargetStateStore.h>
#include <program/Program.h>

#include "ChangeTargetConsistencyStatesMsgEx.h"

bool ChangeTargetConsistencyStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmdt shutting down."));
      return true;
   }

   MgmtdTargetStateStore* stateStore;
   MirrorBuddyGroupMapperEx* mirrorBuddyGroupMapper;

   NodeType nodeType = getNodeType();

   switch (nodeType)
   {
      case NODETYPE_Storage:
         mirrorBuddyGroupMapper = app->getStorageBuddyGroupMapper();
         stateStore = app->getTargetStateStore();
         break;

      case NODETYPE_Meta:
         mirrorBuddyGroupMapper = app->getMetaBuddyGroupMapper();
         stateStore = app->getMetaStateStore();
         break;

      default:
         LOG(STATES, ERR, "Invalid node type.", nodeType, ("Sender", ctx.peerName()));
         return false;
   }

   FhgfsOpsErr result = FhgfsOpsErr_UNKNOWNTARGET;

   result = stateStore->changeConsistencyStatesFromLists(getTargetIDs(), getOldStates(),
      getNewStates(), mirrorBuddyGroupMapper);

   ctx.sendResponse(ChangeTargetConsistencyStatesRespMsg(result) );

   stateStore->saveStatesToFile();

   return true;
}
