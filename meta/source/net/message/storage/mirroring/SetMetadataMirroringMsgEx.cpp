#include <common/net/message/nodes/SetRootNodeIDMsg.h>
#include <common/net/message/nodes/SetRootNodeIDRespMsg.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/MetaStorageTk.h>
#include <storage/DirInode.h>
#include <storage/MetaStore.h>
#include <toolkit/BuddyCommTk.h>

#include <program/Program.h>

#include "SetMetadataMirroringMsgEx.h"


bool SetMetadataMirroringMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("SetMetadataMirroringMsgEx incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a SetMetadataMirroringMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

   // verify that the current node is in a group, and is the primary for its group
   bool localNodeIsPrimary;
   uint16_t buddyGroupID = metaBuddyGroupMapper->getBuddyGroupID(app->getLocalNodeNumID().val(),
      &localNodeIsPrimary);

   if (buddyGroupID == 0)
   {
      LogContext(__func__).logErr("This node is not part of a buddy group.");
      ctx.sendResponse(SetMetadataMirroringRespMsg(FhgfsOpsErr_INTERNAL));
      return true;
   }

   if (!localNodeIsPrimary)
   {
      LogContext(__func__).logErr("This node is not the primary root node.");
      ctx.sendResponse(SetMetadataMirroringRespMsg(FhgfsOpsErr_INTERNAL));
      return true;
   }

   // verify owner of root dir
   if (app->getLocalNodeNumID() != app->getRootDir()->getOwnerNodeID())
   {
      LogContext(__func__).logErr("This node does not own the root directory.");
      ctx.sendResponse(SetMetadataMirroringRespMsg(FhgfsOpsErr_NOTOWNER));
      return true;
   }

   FhgfsOpsErr setRes = setMirroring();

   if (setRes == FhgfsOpsErr_SUCCESS)
      BuddyCommTk::setBuddyNeedsResyncState(true);

   ctx.sendResponse(SetMetadataMirroringRespMsg(setRes) );

   return true;
}

FhgfsOpsErr SetMetadataMirroringMsgEx::setMirroring()
{
   // no two threads must be allowed to run this code at the same time. this could happen if a user
   // starts multiple ctl instances, or during bulk resync.
   static Mutex setMirrorMtx;
   std::unique_lock<Mutex> setMirrorMtxLock(setMirrorMtx);

   // more than one thread may have called this method. if so, report success to the ones who waited
   if (Program::getApp()->getRootDir()->getIsBuddyMirrored())
      return FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

   NumNodeID localNodeNumID = app->getLocalNodeNumID();

   // get buddy group for this node
   bool localNodeIsPrimary;
   uint16_t buddyGroupID = metaBuddyGroupMapper->getBuddyGroupID(localNodeNumID.val(),
      &localNodeIsPrimary);

   // move inode of root directory to mirrored dir
   FhgfsOpsErr mvInodeRes;
   FhgfsOpsErr mvDirRes;

   mvInodeRes = moveRootInode(false);

   if (mvInodeRes != FhgfsOpsErr_SUCCESS)
      return FhgfsOpsErr_INTERNAL;

   // move root directory to mirrored dir
   mvDirRes = moveRootDirectory(false);
   if (mvDirRes != FhgfsOpsErr_SUCCESS)
   {
      // get inode back
      moveRootInode(true);

      return FhgfsOpsErr_INTERNAL;
   }

   // update buddy mirror info and write to disk
   // NOTE: this must happen after the data has been moved, because buddy mirror flag changes save
   // path inside of DirInode object
   DirInode* dir = app->getRootDir();

   const FhgfsOpsErr setMirrorRes = dir->setAndStoreIsBuddyMirrored(true);
   if (setMirrorRes != FhgfsOpsErr_SUCCESS)
   {
      LOG(MIRRORING, ERR, "Could not set mirror state on root inode", setMirrorRes);
      const FhgfsOpsErr revertSetRes = dir->setAndStoreIsBuddyMirrored(false);
      if (revertSetRes != FhgfsOpsErr_SUCCESS)
         LOG(MIRRORING, ERR, "Could not revert mirror setting either, your filesystem is now corrupt",
               revertSetRes);

      return FhgfsOpsErr_SAVEERROR;
   }

   bool setOwnerRes = dir->setOwnerNodeID(NumNodeID(buddyGroupID) );

   if (!setOwnerRes)
   {
      // get inode back
      moveRootInode(true);

      // get dir back
      moveRootDirectory(true);

      return FhgfsOpsErr_INTERNAL;
   }

   // update root Node in meta store
   bool updateNodeIDRes = app->getMetaNodes()->setRootNodeNumID(NumNodeID(buddyGroupID), true,
      true);
   if (!updateNodeIDRes)
   {
      // get inode back
      moveRootInode(true);

      // get dir back
      moveRootDirectory(true);

      // set old owner again and remove buddy mirror flag
      dir->setAndStoreIsBuddyMirrored(false);
      dir->setOwnerNodeID(localNodeNumID);

      return FhgfsOpsErr_INTERNAL;
   }

   FhgfsOpsErr informMgmtRes = informMgmt(buddyGroupID);

   if (informMgmtRes != FhgfsOpsErr_SUCCESS)
   {
      // get inode back
      moveRootInode(true);

      // get dir back
      moveRootDirectory(true);

      // set old owner again and remove buddy mirror flag
      dir->setAndStoreIsBuddyMirrored(false);
      dir->setOwnerNodeID(localNodeNumID);

      // set old root in store
      app->getMetaNodes()->setRootNodeNumID(localNodeNumID, true, false);

      return informMgmtRes;
   }

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr SetMetadataMirroringMsgEx::moveRootInode(bool revertMove)
{
   App* app = Program::getApp();

   Path oldPath( app->getMetaPath() + "/"
      + MetaStorageTk::getMetaInodePath(app->getInodesPath()->str(),
         META_ROOTDIR_ID_STR));

   Path newPath( app->getMetaPath() + "/"
      + MetaStorageTk::getMetaInodePath(app->getBuddyMirrorInodesPath()->str(),
         META_ROOTDIR_ID_STR));

   int renameRes;
   if ( !revertMove )
   {
      StorageTk::createPathOnDisk(newPath, true);
      renameRes = rename(oldPath.str().c_str(), newPath.str().c_str());
   }
   else
      renameRes = rename(newPath.str().c_str(), oldPath.str().c_str());

   if ( renameRes )
   {
      LogContext(__func__).logErr(
         "Unable to move root inode; error: " + System::getErrString());

      return FhgfsOpsErr_INTERNAL;
   }

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr SetMetadataMirroringMsgEx::moveRootDirectory(bool revertMove)
{
   App* app = Program::getApp();

   Path oldPath(app->getMetaPath() + "/"
      + MetaStorageTk::getMetaDirEntryPath(app->getDentriesPath()->str(),
         META_ROOTDIR_ID_STR));
   Path newPath(app->getMetaPath() + "/"
      + MetaStorageTk::getMetaDirEntryPath(app->getBuddyMirrorDentriesPath()->str(),
         META_ROOTDIR_ID_STR));

   int renameRes;
   if ( !revertMove )
   {
      StorageTk::createPathOnDisk(newPath, true);
      renameRes = rename(oldPath.str().c_str(), newPath.str().c_str());
   }
   else
      renameRes = rename(newPath.str().c_str(), oldPath.str().c_str());

   if (renameRes)
   {
      LogContext(__func__).logErr(
         "Unable to move root directory; error: " + System::getErrString());

      return FhgfsOpsErr_INTERNAL;
   }

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr SetMetadataMirroringMsgEx::informMgmt(uint16_t newRootNodeID)
{
   FhgfsOpsErr result = FhgfsOpsErr_SUCCESS;
   NodeStore* mgmtdNodes = Program::getApp()->getMgmtNodes();

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   SetRootNodeIDRespMsg* respMsgCast;
   FhgfsOpsErr setIDRes;

   SetRootNodeIDMsg setRootNodeIDMsg(newRootNodeID, true);

   auto mgmtNode = mgmtdNodes->referenceFirstNode();

   // request/response
   commRes = MessagingTk::requestResponse(*mgmtNode, &setRootNodeIDMsg,
      NETMSGTYPE_SetRootNodeIDResp, &respBuf, &respMsg);
   if ( !commRes )
   {
      LogContext(__func__).logErr("Communication with management node failed.");
      result = FhgfsOpsErr_COMMUNICATION;

      goto err_cleanup;
   }

   respMsgCast = (SetRootNodeIDRespMsg*) respMsg;

   setIDRes = (FhgfsOpsErr) respMsgCast->getValue();
   if ( setIDRes != FhgfsOpsErr_SUCCESS )
   {
      LogContext(__func__).logErr("Failed to set new root node ID on management.");
      result = setIDRes;
   }

   err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return result;
}
