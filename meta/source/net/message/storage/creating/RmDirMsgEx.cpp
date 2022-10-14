#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/creating/RmDirRespMsg.h>
#include <common/net/message/storage/creating/RmLocalDirMsg.h>
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>
#include <components/ModificationEventFlusher.h>
#include <program/Program.h>
#include <session/EntryLock.h>
#include "RmDirMsgEx.h"


bool RmDirMsgEx::processIncoming(ResponseContext& ctx)
{
   BaseType::processIncoming(ctx);

   // update operation counters
   updateNodeOp(ctx, MetaOpCounter_RMDIR);

   return true;
}

std::tuple<HashDirLock, FileIDLock, FileIDLock, ParentNameLock> RmDirMsgEx::lock(EntryLockStore& store)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryInfo delEntryInfo;

   DirInode* parentDir = metaStore->referenceDir(
         getParentInfo()->getEntryID(), getParentInfo()->getIsBuddyMirrored(), true);
   if (!parentDir)
      return {};
   else
   {
      parentDir->getDirEntryInfo(getDelDirName(), delEntryInfo);
      metaStore->releaseDir(getParentInfo()->getEntryID());
   }

   FileIDLock parentDirLock;
   FileIDLock delDirLock;
   HashDirLock hashLock;

   if (resyncJob && resyncJob->isRunning())
      hashLock = {&store, MetaStorageTk::getMetaInodeHash(delEntryInfo.getEntryID())};

   // lock directories in deadlock-avoidance order, see MirroredMessage::lock()
   if (delEntryInfo.getEntryID().empty())
   {
      parentDirLock = {&store, getParentInfo()->getEntryID(), true};
   }
   else if (delEntryInfo.getEntryID() < getParentInfo()->getEntryID())
   {
      delDirLock = {&store, delEntryInfo.getEntryID(), true};
      parentDirLock = {&store, getParentInfo()->getEntryID(), true};
   }
   else if (delEntryInfo.getEntryID() == getParentInfo()->getEntryID())
   {
      delDirLock = {&store, delEntryInfo.getEntryID(), true};
   }
   else
   {
      parentDirLock = {&store, getParentInfo()->getEntryID(), true};
      delDirLock = {&store, delEntryInfo.getEntryID(), true};
   }

   ParentNameLock parentNameLock(&store, getParentInfo()->getEntryID(), getDelDirName());

   return std::make_tuple(
         std::move(hashLock),
         std::move(parentDirLock),
         std::move(delDirLock),
         std::move(parentNameLock));
}

std::unique_ptr<RmDirMsgEx::ResponseState> RmDirMsgEx::rmDir(ResponseContext& ctx,
   const bool isSecondary)
{
   App* app = Program::getApp();
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   ModificationEventFlusher* modEventFlusher = Program::getApp()->getModificationEventFlusher();
   bool modEventLoggingEnabled = modEventFlusher->isLoggingEnabled();

   FhgfsOpsErr retVal;

   EntryInfo* parentInfo = this->getParentInfo();
   const std::string& delDirName = this->getDelDirName();

   EntryInfo outDelEntryInfo;

   // reference parent
   DirInode* parentDir = metaStore->referenceDir(parentInfo->getEntryID(),
      parentInfo->getIsBuddyMirrored(), true);
   if(!parentDir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   DirEntry removeDirEntry(delDirName);
   bool getEntryRes = parentDir->getDirDentry(delDirName, removeDirEntry);
   if(!getEntryRes)
      retVal = FhgfsOpsErr_PATHNOTEXISTS;
   else
   {
      int additionalFlags = 0;

      const std::string& parentEntryID = parentInfo->getEntryID();
      removeDirEntry.getEntryInfo(parentEntryID, additionalFlags, &outDelEntryInfo);

      App* app = Program::getApp();
      NumNodeID ownerNodeID = outDelEntryInfo.getOwnerNodeID();

      // no-comms path: the dir inode is owned by us
      if ((!isMirrored() && ownerNodeID == app->getLocalNode().getNumID()) ||
            (isMirrored() &&
               ownerNodeID.val() == app->getMetaBuddyGroupMapper()->getLocalGroupID()))
         retVal = app->getMetaStore()->removeDirInode(outDelEntryInfo.getEntryID(),
               outDelEntryInfo.getIsBuddyMirrored());
      else if (!isSecondary)
         retVal = rmRemoteDirInode(&outDelEntryInfo);
      else
         retVal = FhgfsOpsErr_SUCCESS;

      if(retVal == FhgfsOpsErr_SUCCESS)
      { // local removal succeeded => remove meta dir
         retVal = parentDir->removeDir(delDirName, NULL);
      }
   }

   if (retVal == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(*parentDir, dirTimestamps);

   // clean-up
   metaStore->releaseDir(parentInfo->getEntryID() );

   if (!isSecondary && app->getFileEventLogger() && getFileEvent())
   {
         app->getFileEventLogger()->log(
                  *getFileEvent(),
                  outDelEntryInfo.getEntryID(),
                  outDelEntryInfo.getParentEntryID());
   }

   if (retVal == FhgfsOpsErr_SUCCESS && modEventLoggingEnabled)
   {
      const std::string& entryID = outDelEntryInfo.getEntryID();
      modEventFlusher->add(ModificationEvent_DIRREMOVED, entryID);
   }

   return boost::make_unique<ResponseState>(retVal);
}

/**
 * Remove the inode of this directory
 */
FhgfsOpsErr RmDirMsgEx::rmRemoteDirInode(EntryInfo* delEntryInfo)
{
   const char* logContext = "RmDirMsg (rm dir inode)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   NumNodeID ownerNodeID = delEntryInfo->getOwnerNodeID();

   LOG_DEBUG(logContext, Log_DEBUG,
      "Removing dir inode from metadata node: " + ownerNodeID.str() + "; "
      "dirname: " + delEntryInfo->getFileName() + "; isBuddyMirrored: " +
      StringTk::intToStr(delEntryInfo->getIsBuddyMirrored()) );

   // prepare request

   RmLocalDirMsg rmMsg(delEntryInfo);

   RequestResponseArgs rrArgs(NULL, &rmMsg, NETMSGTYPE_RmLocalDirResp);

   RequestResponseNode rrNode(ownerNodeID, app->getMetaNodes() );
   rrNode.setTargetStates(app->getMetaStateStore() );

   if (delEntryInfo->getIsBuddyMirrored())
      rrNode.setMirrorInfo(app->getMetaBuddyGroupMapper(), false);

   do // (this loop just exists to enable the "break"-jump, so it's not really a loop)
   {
      // send request to other mds and receive response

      FhgfsOpsErr requestRes = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

      if(requestRes != FhgfsOpsErr_SUCCESS)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with metadata server failed. "
            "ownerNodeID: " + ownerNodeID.str() + "; "
            "dirID: "       + delEntryInfo->getEntryID()       + "; " +
            "dirname: "     + delEntryInfo->getFileName() );
         retVal = requestRes;
         break;
      }

      // correct response type received
      const auto rmRespMsg = (const RmLocalDirRespMsg*)rrArgs.outRespMsg.get();

      retVal = rmRespMsg->getResult();
      if(unlikely( (retVal != FhgfsOpsErr_SUCCESS) && (retVal != FhgfsOpsErr_NOTEMPTY) ) )
      { // error: dir inode not removed
         std::string errString = boost::lexical_cast<std::string>(retVal);

         LogContext(logContext).log(Log_DEBUG,
            "Metadata server was unable to remove dir inode. "
            "ownerNodeID: " + ownerNodeID.str() + "; "
            "dirID: "       + delEntryInfo->getEntryID()       + "; " +
            "dirname: "     + delEntryInfo->getFileName()      + "; " +
            "error: "       + errString);

         break;
      }

      // success: dir inode removed
      LOG_DEBUG(logContext, Log_DEBUG,
         "Metadata server removed dir inode: "
         "ownerNodeID: " + ownerNodeID.str() + "; "
         "dirID: "       + delEntryInfo->getEntryID()       + "; " +
         "dirname: "     + delEntryInfo->getFileName() );

   } while(false);


   return retVal;
}

void RmDirMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_RmDirResp);
}
