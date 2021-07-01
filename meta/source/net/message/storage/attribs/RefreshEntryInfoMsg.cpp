#include <common/net/message/storage/attribs/RefreshEntryInfoRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperStat.h>
#include <common/storage/EntryInfo.h>
#include <program/Program.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>
#include "RefreshEntryInfoMsgEx.h"


bool RefreshEntryInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RefreshEntryInfoMsgEx incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a RefreshEntryInfoMsg from: " + ctx.peerName() );

   LOG_DEBUG("RefreshEntryInfoMsgEx::processIncoming", Log_SPAM,
      "ParentID: " + getEntryInfo()->getParentEntryID() + " EntryID: " +
      getEntryInfo()->getEntryID() + " BuddyMirrored: " +
      (getEntryInfo()->getIsBuddyMirrored() ? "Yes" : "No") + " Secondary: " +
      (hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) ? "Yes" : "No"));

   BaseType::processIncoming(ctx);

   updateNodeOp(ctx, MetaOpCounter_REFRESHENTRYINFO);

   return true;
}

std::tuple<FileIDLock, FileIDLock> RefreshEntryInfoMsgEx::lock(EntryLockStore& store)
{
   if (DirEntryType_ISDIR(getEntryInfo()->getEntryType()))
      return std::make_tuple(
            FileIDLock(),
            FileIDLock(&store, getEntryInfo()->getEntryID(), true));
   else
      return std::make_tuple(
            FileIDLock(&store, getEntryInfo()->getEntryID(), true),
            FileIDLock());
}

std::unique_ptr<MirroredMessageResponseState> RefreshEntryInfoMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   if (getEntryInfo()->getParentEntryID().empty()) // special case: get info for root directory
      return boost::make_unique<ResponseState>(refreshInfoRoot());
   else
      return boost::make_unique<ResponseState>(refreshInfoRec());
}

/**
 * @param outPattern StripePattern clone (in case of success), that must be deleted by the caller
 */
FhgfsOpsErr RefreshEntryInfoMsgEx::refreshInfoRec()
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   EntryInfo* entryInfo = getEntryInfo();

   DirInode* dir = metaStore->referenceDir(entryInfo->getEntryID(),
      entryInfo->getIsBuddyMirrored(), true);
   if(dir)
   { // entry is a directory
      dir->refreshMetaInfo();
      metaStore->releaseDir(entryInfo->getEntryID() );
      return FhgfsOpsErr_SUCCESS;
   }

   // not a dir => try file
   auto refreshRes = MsgHelperStat::refreshDynAttribs(entryInfo, true, getMsgHeaderUserID());
   if (refreshRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
   {
      auto file = metaStore->referenceFile(getEntryInfo());
      if (file)
         fixInodeTimestamp(*file, fileTimestamps, getEntryInfo());

      metaStore->releaseFile(getEntryInfo()->getParentEntryID(), file);
   }

   return refreshRes;
}

FhgfsOpsErr RefreshEntryInfoMsgEx::refreshInfoRoot()
{
   App* app = Program::getApp();
   DirInode* rootDir = app->getRootDir();

   NumNodeID expectedOwnerNode = rootDir->getIsBuddyMirrored()
      ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() )
      : app->getLocalNode().getNumID();

   if ( expectedOwnerNode != rootDir->getOwnerNodeID() )
      return FhgfsOpsErr_NOTOWNER;

   rootDir->refreshMetaInfo();

   return FhgfsOpsErr_SUCCESS;
}

void RefreshEntryInfoMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_RefreshEntryInfoResp);
}
