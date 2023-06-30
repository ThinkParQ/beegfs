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
         fileLock = {&store, fromInfo.getEntryID(), true};

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

   if (!toDir)
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
      // if file Inode is already de-inlined then do not check/deinline again
      if (!getFromInfo()->getIsInlined())
      {
         retVal = FhgfsOpsErr_SUCCESS;
      }
      else
      {
         DirInode* fromDir = metaStore->referenceDir(getFromDirInfo()->getEntryID(),
               getFromDirInfo()->getIsBuddyMirrored(), true);

         if (!fromDir)
         {
            LogContext(logContext).logErr("Parent dir of source file not exists."
               "dirname: " + getFromDirInfo()->getFileName());

            metaStore->releaseDir(toDir->getID());
            return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);
         }

         retVal = metaStore->verifyAndMoveFileInode(*fromDir, getFromInfo(), MODE_DEINLINE);
         metaStore->releaseDir(fromDir->getID());
      }

      if (retVal == FhgfsOpsErr_SUCCESS)
      {
         retVal = metaStore->incDecLinkCount(getFromInfo(), 1);
      }

      if (retVal != FhgfsOpsErr_SUCCESS)
      {
         // release toDir as well and return error without writting new dentry
         metaStore->releaseDir(toDir->getID());
         return boost::make_unique<ResponseState>(FhgfsOpsErr_INTERNAL);
      }
   }
   else if (!isSecondary)
   {
      if (!getFromInfo()->getIsInlined())
      {
         retVal = FhgfsOpsErr_SUCCESS;
      }
      else
      {
         MoveFileInodeMsg deinlineMsg(getFromInfo(), MODE_DEINLINE);

         RequestResponseArgs rrArgs(NULL, &deinlineMsg, NETMSGTYPE_MoveFileInodeResp);
         RequestResponseNode rrNode(ownerNodeID, app->getMetaNodes());
         if (getFromDirInfo()->getIsBuddyMirrored())
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
               LogContext(logContext).logErr(std::string("Communication with metadata server failed. ") +
                  "nodeID: " + ownerNodeID.str() + "; " +
                  "file: " + getFromInfo()->getFileName().c_str());
               retVal = resp;
               break;
            }

            // response received
            const auto moveFileInodeRespMsg = (MoveFileInodeRespMsg*) rrArgs.outRespMsg.get();
            FhgfsOpsErr res = moveFileInodeRespMsg->getResult();
            if (res != FhgfsOpsErr_SUCCESS)
            {
               // error: either source file not exists or deinline operation failed
               LogContext(logContext).logErr("verify and move file inode failed! "
                  "nodeID: " + ownerNodeID.str() + "; " +
                  "fileName: " + getFromInfo()->getFileName().c_str());

               retVal = res;
               break;
            }

            // success
            retVal = res;
         } while (false);
      }

      if (retVal == FhgfsOpsErr_SUCCESS)
      {
         retVal = incDecRemoteLinkCount(ownerNodeID, true);
         if (retVal != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).logErr(std::string("inc link count failed on remote"
               "inode, ownerNodeID: " + ownerNodeID.str()));
         }
      }
   }

   // If verify/deinline and link count update is successful - only then create new
   // dentry in "toDir" with entryID and ownerNodeID of "fromFile" which might reside
   // on another meta node/buddy-group
   if (retVal == FhgfsOpsErr_SUCCESS)
   {
      DirEntry newHardlink(DirEntryType_REGULARFILE, getToName(),
         getFromInfo()->getEntryID(), getFromInfo()->getOwnerNodeID());

      newHardlink.removeDentryFeatureFlag(DENTRY_FEATURE_INODE_INLINE);

      if (isMirrored())
         newHardlink.setBuddyMirrorFeatureFlag();

      FhgfsOpsErr makeRes = toDir->makeDirEntry(newHardlink);

      if (makeRes != FhgfsOpsErr_SUCCESS)
      {
         // compensate link count if dentry create fails
         if (!isLocalOwner)
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
           metaStore->releaseFile(getFromDirInfo()->getEntryID(), file);
        }
      }
   }

   metaStore->releaseDir(toDir->getID());

   if (retVal == FhgfsOpsErr_SUCCESS && app->getFileEventLogger() && getFileEvent())
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

   if (getFromDirInfo()->getIsBuddyMirrored())
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
            "nodeID: " + ownerNodeID.str() + "; " +
            "file: " + getFromInfo()->getFileName().c_str());
         retVal = resp;
         break;
      }

      // response received
      const auto incLinkCountResp = (SetAttrRespMsg*) rrArgs.outRespMsg.get();
      FhgfsOpsErr res = static_cast<FhgfsOpsErr>(incLinkCountResp->getValue());
      if (res != FhgfsOpsErr_SUCCESS)
      {
         // error: either source file not exists or nlink count increment failed
         LogContext(logContext).logErr("nLink count increment failed! "
            "nodeID: " + ownerNodeID.str() + "; " +
            "fileName: " + getFromInfo()->getFileName().c_str());

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
