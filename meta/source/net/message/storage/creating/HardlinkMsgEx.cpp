#include <program/Program.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/creating/HardlinkRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>

#include "HardlinkMsgEx.h"

std::tuple<FileIDLock, ParentNameLock, ParentNameLock, FileIDLock> HardlinkMsgEx::lock(
      EntryLockStore& store)
{
   // NOTE: normally we'd need to also lock on the MDS holding the destination file,
   // but we don't support hardlinks to different servers, yet
   ParentNameLock fromLock;
   ParentNameLock toLock;

   FileIDLock fileLock;

   FileIDLock dirLock(&store, getToDirInfo()->getEntryID(), true);

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

   // Acquire the inode lock because hardlinking modifies the link count.
   fileLock = {&store, getFromInfo()->getEntryID(), true};

   return std::make_tuple(
         std::move(dirLock),
         std::move(fromLock),
         std::move(toLock),
         std::move(fileLock));
}

bool HardlinkMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(GENERAL, DEBUG, "",
         ("fromDirID", getFromDirInfo()->getEntryID()),
         ("fromID", getFromInfo()->getEntryID()),
         ("fromName", getFromName()),
         ("toDirID", getToDirInfo()->getEntryID()),
         ("toName", getToName()),
         ("buddyMirrored", getToDirInfo()->getIsBuddyMirrored()));

   BaseType::processIncoming(ctx);

   updateNodeOp(ctx, MetaOpCounter_HARDLINK);

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

   if (!isSecondary && retVal == FhgfsOpsErr_SUCCESS && app->getFileEventLogger() && getFileEvent())
   {
         app->getFileEventLogger()->log(
                  *getFileEvent(),
                  getFromInfo()->getEntryID(),
                  getFromInfo()->getParentEntryID());
   }

   return boost::make_unique<ResponseState>(retVal);
}

void HardlinkMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_HardlinkResp);
}
