#include <program/Program.h>
#include <common/net/message/storage/attribs/UpdateDirParentRespMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/worker/SetChunkFileAttribsWork.h>
#include <session/EntryLock.h>
#include "UpdateDirParentMsgEx.h"


bool UpdateDirParentMsgEx::processIncoming(ResponseContext& ctx)
{
   EntryInfo* entryInfo = getEntryInfo();
   NumNodeID parentNodeID = getParentNodeID();

   LOG_DEBUG("UpdateDirParentMsgEx::processIncoming", Log_DEBUG,
      "ParentID: " + entryInfo->getParentEntryID() + " EntryID: " +
      entryInfo->getEntryID() + " parentNodeID: " + parentNodeID.str() + " BuddyMirrored: " +
      (entryInfo->getIsBuddyMirrored() ? "Yes" : "No") + " Secondary: " +
      (hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) ? "Yes" : "No"));
   (void) entryInfo;
   (void) parentNodeID;

   rctx = &ctx;

   BaseType::processIncoming(ctx);

   // update operation counters
   updateNodeOp(ctx, MetaOpCounter_UPDATEDIRPARENT);

   return true;
}

FileIDLock UpdateDirParentMsgEx::lock(EntryLockStore& store)
{
   if (rctx->isLocallyGenerated())
      return {};

   return {&store, getEntryInfo()->getEntryID(), true};
}

std::unique_ptr<MirroredMessageResponseState> UpdateDirParentMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   auto setRes = Program::getApp()->getMetaStore()->setDirParent(getEntryInfo(), getParentNodeID());

   if (setRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
   {
      auto dir = Program::getApp()->getMetaStore()->referenceDir(getEntryInfo()->getEntryID(),
            true, true);
      if (dir)
      {
         fixInodeTimestamp(*dir, dirTimestamps);
         Program::getApp()->getMetaStore()->releaseDir(dir->getID());
      }
   }

   return boost::make_unique<ResponseState>(setRes);
}

void UpdateDirParentMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_UpdateDirParentResp);
}
