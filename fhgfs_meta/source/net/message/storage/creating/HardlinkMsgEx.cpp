#include <program/Program.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/UpdateBacklinkMsg.h>
#include <common/net/message/storage/attribs/UpdateBacklinkRespMsg.h>
#include <common/net/message/storage/creating/HardlinkRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperChunkBacklinks.h>

#include "HardlinkMsgEx.h"

std::tuple<DirIDLock, ParentNameLock, ParentNameLock, FileIDLock> HardlinkMsgEx::lock(
      EntryLockStore& store)
{
   // NOTE: normally we'd need to also lock on the MDS holding the destination file,
   // but we don't support hardlinks to different servers, yet
   ParentNameLock fromLock;
   ParentNameLock toLock;

   FileIDLock fileLock;

   DirIDLock dirLock(&store, getToDirInfo()->getEntryID(), true);

   // take care about lock ordering! see MirroredMessage::lock()
   if (getFromDirInfo()->getEntryID() < getToDirInfo()->getEntryID()
         || (getFromDirInfo()->getEntryID() == getToDirInfo()->getEntryID()
               && getFromName() < getToName()))
   {
      fromLock = {&store, getFromDirInfo()->getEntryID(), getFromName()};
      toLock = {&store, getToDirInfo()->getEntryID(), getToName()};
   }
   else if (getFromDirInfo()->getEntryID() == getToDirInfo()->getEntryID()
         && getFromName() == getToName())
   {
      fromLock = {&store, getFromDirInfo()->getEntryID(), getFromName()};
   }
   else
   {
      toLock = {&store, getToDirInfo()->getEntryID(), getToName()};
      fromLock = {&store, getFromDirInfo()->getEntryID(), getFromName()};
   }

   // hardlinking modifies the file inode link count, so we have to lock the file. if we can't
   // reference the directory, or if the file does not exist, we can continue - the directory is
   // locked, so the file cannot suddenly appear after we return here.
   auto dir = Program::getApp()->getMetaStore()->referenceDir(getFromDirInfo()->getEntryID(),
         getFromInfo()->getIsBuddyMirrored(), true);
   if (dir)
   {
      EntryInfo fromInfo;

      dir->getFileEntryInfo(getFromName(), fromInfo);
      if (DirEntryType_ISFILE(fromInfo.getEntryType()))
         fileLock = {&store, fromInfo.getEntryID()};

      Program::getApp()->getMetaStore()->releaseDir(dir->getID());
   }

   return std::make_tuple(
         std::move(dirLock),
         std::move(fromLock),
         std::move(toLock),
         std::move(fileLock));
}

bool HardlinkMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(GENERAL, DEBUG, "Received a HardlinkMsg", as("peer", ctx.peerName()),
         as("fromDirID", getFromDirInfo()->getEntryID()),
         as("fromID", getFromInfo()->getEntryID()),
         as("fromName", getFromName()),
         as("toDirID", getToDirInfo()->getEntryID()),
         as("toName", getToName()),
         as("buddyMirrored", getToDirInfo()->getIsBuddyMirrored()));

   BaseType::processIncoming(ctx);

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_HARDLINK,
      getMsgHeaderUserID());

   return true;
}

std::unique_ptr<MirroredMessageResponseState> HardlinkMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   const char* logContext = "create hard link";
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   // this should never happen, because we already check on client
   if (unlikely(getFromDirInfo()->getEntryID() != getToDirInfo()->getEntryID()))
      return boost::make_unique<ResponseState>(FhgfsOpsErr_INTERNAL);

   // simple and cheap link (everything local)

   LOG_DEBUG(logContext, Log_SPAM, "Method: link in same dir");// debug in
   IGNORE_UNUSED_VARIABLE(logContext);

   DirInode* parentDir = metaStore->referenceDir(getFromDirInfo()->getEntryID(),
         getFromDirInfo()->getIsBuddyMirrored(), true);
   if (!parentDir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   retVal = metaStore->linkInSameDir(*parentDir, getFromInfo(), getFromName(), getToName());

   if (retVal == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
   {
      fixInodeTimestamp(*parentDir, dirTimestamps);

      auto file = metaStore->referenceFile(getFromInfo());
      if (file)
      {
         fixInodeTimestamp(*file, fileTimestamps, getFromInfo());
         metaStore->releaseFile(getFromDirInfo()->getEntryID(), file);
      }
   }

   metaStore->releaseDir(parentDir->getID());

   if (!isSecondary && retVal == FhgfsOpsErr_SUCCESS && getFileEvent() && app->getFileEventLogger())
   {
      auto& ev = *getFileEvent();
      app->getFileEventLogger()->log(ev, false);
   }

   return boost::make_unique<ResponseState>(retVal);
}

void HardlinkMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_HardlinkResp);
}
