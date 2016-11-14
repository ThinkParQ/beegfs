#include <common/net/message/control/GenericResponseMsg.h>
#include <program/Program.h>
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>

#include "MkLocalDirMsgEx.h"

std::tuple<> MkLocalDirMsgEx::lock(EntryLockStore& store)
{
   // we need not lock anything here, because the inode ID will be completely unknown to anyone
   // until we finish processing here *and* on the metadata server that sent this message.
   return {};
}

bool MkLocalDirMsgEx::processIncoming(ResponseContext& ctx)
{
   EntryInfo* entryInfo = getEntryInfo();

   LOG_DBG(DEBUG, "Received a MkLocalDirMsg", as("peer", ctx.peerName()),
         as("entryID", entryInfo->getEntryID()),
         as("entryName", entryInfo->getFileName()));
   (void) entryInfo;

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> MkLocalDirMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   StripePattern& pattern = getPattern();

   EntryInfo *entryInfo = getEntryInfo();
   NumNodeID parentNodeID = getParentNodeID();

   NumNodeID ownerNodeID = entryInfo->getIsBuddyMirrored()
      ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() )
      : app->getLocalNode().getNumID();

   DirInode newDir(entryInfo->getEntryID(), getMode(), getUserID(),
      getGroupID(), ownerNodeID, pattern, entryInfo->getIsBuddyMirrored());

   newDir.setParentInfoInitial(entryInfo->getParentEntryID(), parentNodeID);

   FhgfsOpsErr mkRes = metaStore->makeDirInode(newDir, getDefaultACLXAttr(), getAccessACLXAttr() );

   if (mkRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(newDir, dirTimestamps);

   return boost::make_unique<ResponseState>(mkRes);
}

bool MkLocalDirMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   return sendToSecondary(ctx, *this, NETMSGTYPE_MkLocalDirResp) == FhgfsOpsErr_SUCCESS;
}
