#include <program/Program.h>
#include <common/net/message/storage/creating/HardlinkRespMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>
#include <common/net/message/storage/attribs/SetAttrMsg.h>
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
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

   // We need to lock the inode because hardlinking modifies the link count.
   // If the file inode is present on the current meta node or buddy-group,
   // then acquire the inodeLock.
   App* app = Program::getApp();
   NumNodeID ownerNodeID = getFromInfo()->getOwnerNodeID();
   bool isLocalInode = ((!isMirrored() && ownerNodeID == app->getLocalNode().getNumID()) ||
         (isMirrored() && ownerNodeID.val() == app->getMetaBuddyGroupMapper()->getLocalGroupID()));

   if (isLocalInode)
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

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   // reference directory where hardlink needs to be created
   DirInode* toDir = metaStore->referenceDir(getToDirInfo()->getEntryID(),
         getToDirInfo()->getIsBuddyMirrored(), true);

   // check if file dentry already exists with same name as hardlink
   // return error if link already exists
   if (likely(toDir))
   {
      DirEntry newLink(getToName());
      if (toDir->getFileDentry(getToName(), newLink))
      {
         metaStore->releaseDir(toDir->getID());
         return boost::make_unique<ResponseState>(FhgfsOpsErr_EXISTS);
      }
   }
   else
   {
      LogContext(logContext).logErr(std::string("target directory for hard-link doesn't exist,"
         "dirname: " + getToDirInfo()->getFileName()));
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);
   }

   // check whether local node/group owns source file's inode or not
   NumNodeID ownerNodeID = getFromInfo()->getOwnerNodeID();
   bool isLocalOwner = ((!isMirrored() && ownerNodeID == app->getLocalNode().getNumID()) ||
         (isMirrored() && ownerNodeID.val() == app->getMetaBuddyGroupMapper()->getLocalGroupID()));

   if (isLocalOwner)
   {
      retVal = metaStore->makeNewHardlink(getFromInfo());
   }
   else if (!isSecondary)
   {
      MoveFileInodeMsg deinlineMsg(getFromInfo(), MODE_DEINLINE, true);

      RequestResponseArgs rrArgs(NULL, &deinlineMsg, NETMSGTYPE_MoveFileInodeResp);
      RequestResponseNode rrNode(ownerNodeID, app->getMetaNodes());
      rrNode.setTargetStates(app->getMetaStateStore());

      if (getFromInfo()->getIsBuddyMirrored())
      {
         rrNode.setMirrorInfo(app->getMetaBuddyGroupMapper(), false);
      }

      do
      {
         // send request to other MDs and receive response
         FhgfsOpsErr resp = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

         if (unlikely(resp != FhgfsOpsErr_SUCCESS))
         {
            // error
            LogContext(logContext).logErr("Communication with metadata server failed. "
               "nodeID: " + ownerNodeID.str());

            retVal = resp;
            break;
         }

         // response received
         const auto moveFileInodeRespMsg = (MoveFileInodeRespMsg*) rrArgs.outRespMsg.get();
         FhgfsOpsErr res = moveFileInodeRespMsg->getResult();
         if (res != FhgfsOpsErr_SUCCESS)
         {
            // error: either source file not exists or deinline operation failed
            LogContext(logContext).logErr("verify and move file inode failed! nodeID: "
               + ownerNodeID.str() + "; entryID: " + getFromInfo()->getEntryID());

            retVal = res;
            break;
         }

         // success
         retVal = res;
      } while (false);
   }

   // If verify/deinline inode and link count update is successful - only then create new
   // dentry in "toDir" with entryID and ownerNodeID of "fromFile" which might reside
   // on another meta node/buddy-group
   if (retVal == FhgfsOpsErr_SUCCESS)
   {
      DirEntry newHardlink(DirEntryType_REGULARFILE, getToName(),
         getFromInfo()->getEntryID(), getFromInfo()->getOwnerNodeID());

      newHardlink.removeDentryFeatureFlag(DENTRY_FEATURE_INODE_INLINE);

      // buddy mirroring is inherited from parent directory
      if (toDir->getIsBuddyMirrored())
         newHardlink.setBuddyMirrorFeatureFlag();

      FhgfsOpsErr makeRes = toDir->makeDirEntry(newHardlink);

      if (makeRes != FhgfsOpsErr_SUCCESS)
      {
         // compensate link count if dentry create fails
         //
         // for a remote inode, Use SetAttrMsg to decrease link count
         // since its a mirrored message so it will update link count on both primary and
         // secondary hence don't send this message from secondary buddy
         if (!isSecondary && !isLocalOwner)
            incDecRemoteLinkCount(ownerNodeID, false);
         else
            metaStore->incDecLinkCount(getFromInfo(), -1);

         retVal = makeRes;
      }
   }

   if (retVal == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
   {
      fixInodeTimestamp(*toDir, dirTimestamps);

      if (isLocalOwner)
      {
        auto file = metaStore->referenceFile(getFromInfo());
        if (file)
        {
           fixInodeTimestamp(*file, fileTimestamps, getFromInfo());
           metaStore->releaseFile(getFromInfo()->getParentEntryID(), file);
        }
      }
   }

   metaStore->releaseDir(toDir->getID());

   if (!isSecondary && retVal == FhgfsOpsErr_SUCCESS && app->getFileEventLogger() && getFileEvent())
   {
         app->getFileEventLogger()->log(
                  *getFileEvent(),
                  getFromInfo()->getEntryID(),
                  getFromInfo()->getParentEntryID());
   }

   return boost::make_unique<ResponseState>(retVal);
}

FhgfsOpsErr HardlinkMsgEx::incDecRemoteLinkCount(NumNodeID const& ownerNodeID, bool increment)
{
   const char* logContext = "incDecRemoteLinkCount";
   App* app = Program::getApp();
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   SettableFileAttribs dummyAttrib;
   SetAttrMsg msg(getFromInfo(), 0, &dummyAttrib);

   if (increment)
   {
      msg.addMsgHeaderFeatureFlag(SETATTRMSG_FLAG_INCR_NLINKCNT);
   }
   else
   {
      msg.addMsgHeaderFeatureFlag(SETATTRMSG_FLAG_DECR_NLINKCNT);
   }

   RequestResponseArgs rrArgs(NULL, &msg, NETMSGTYPE_SetAttrResp);
   RequestResponseNode rrNode(ownerNodeID, app->getMetaNodes());

   rrNode.setTargetStates(app->getMetaStateStore());

   if (getFromInfo()->getIsBuddyMirrored())
   {
      rrNode.setMirrorInfo(app->getMetaBuddyGroupMapper(), false);
   }

   do
   {
      // send request to other MDs and receive response
      FhgfsOpsErr resp = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

      if (unlikely(resp != FhgfsOpsErr_SUCCESS))
      {
         // error
         LogContext(logContext).log(Log_WARNING,
            "Communication with metadata server failed. "
            "nodeID: " + ownerNodeID.str());
         retVal = resp;
         break;
      }

      // response received
      const auto incLinkCountResp = (SetAttrRespMsg*) rrArgs.outRespMsg.get();
      FhgfsOpsErr res = static_cast<FhgfsOpsErr>(incLinkCountResp->getValue());
      if (res != FhgfsOpsErr_SUCCESS)
      {
         // error: either source file not exists or nlink count increment failed
         std::string operationType = (increment ? "increase": "decrease");
         LogContext(logContext).logErr("nLink count " + operationType
            + " failed! nodeID: " + ownerNodeID.str() + "; "
            + "entryID: " + getFromInfo()->getEntryID());

         retVal = res;
         break;
      }

      // success
      retVal = res;
   } while (false);

   return retVal;
}

void HardlinkMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_HardlinkResp);
}
