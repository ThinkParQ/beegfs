#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/MetaStorageTk.h>

#include <program/Program.h>

#include "SetMetadataMirroringMsgEx.h"


bool SetMetadataMirroringMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   auto result = process(*app);

   ctx.sendResponse(SetMetadataMirroringRespMsg(result));
   return true;
}

FhgfsOpsErr SetMetadataMirroringMsgEx::process(App& app)
{
   // no two threads must be allowed to run this code at the same time. this could happen if a user
   // starts multiple ctl instances.
   static Mutex setMirrorMtx;
   std::unique_lock<Mutex> setMirrorMtxLock(setMirrorMtx);

   auto* const bgm = app.getMetaBuddyGroupMapper();
   auto* const metaNodes = app.getMetaNodes();
   const auto metaNode = metaNodes->referenceNode(app.getMetaRoot().getID());

   if (app.getMetaRoot().getIsMirrored())
   {
      LOG(MIRRORING, WARNING, "Attempted to enable mirroring for a mirrored system.");
      return FhgfsOpsErr_INVAL;
   }

   if (!metaNode)
   {
      LOG(MIRRORING, CRITICAL, "Attempted to enable mirroring for system without metadata nodes.");
      return FhgfsOpsErr_INVAL;
   }

   if (bgm->getBuddyGroupID(metaNode->getNumID().val()) == 0)
   {
      LOG(MIRRORING, CRITICAL, "The root metadata node is not part of a buddy group.");
      return FhgfsOpsErr_INTERNAL;
   }

   {
      SetMetadataMirroringMsg metaMsg;

      const auto respMsg = MessagingTk::requestResponse(*metaNode, metaMsg,
            NETMSGTYPE_SetMetadataMirroringResp);
      if (!respMsg)
      {
         LOG(MIRRORING, CRITICAL, "Could not communicate with root metadata node.");
         return FhgfsOpsErr_INTERNAL;
      }

      const auto respMsgCast = (SetMetadataMirroringRespMsg*) respMsg.get();

      const auto setMirrorRes = respMsgCast->getResult();

      if (setMirrorRes != FhgfsOpsErr_SUCCESS)
         return setMirrorRes;
   }

   const auto rootGroup = bgm->getBuddyGroupID(metaNode->getNumID().val());
   app.getMetaRoot().set(NumNodeID(rootGroup), true);

   app.getMetaStateStore()->TargetStateStore::setConsistencyState(
         bgm->getSecondaryTargetID(rootGroup), TargetConsistencyState_NEEDS_RESYNC);

   return FhgfsOpsErr_SUCCESS;
}
