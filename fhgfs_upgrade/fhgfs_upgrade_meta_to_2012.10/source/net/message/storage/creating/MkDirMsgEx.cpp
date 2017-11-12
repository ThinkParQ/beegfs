#include <program/Program.h>
#include <common/net/message/storage/creating/MkLocalDirMsg.h>
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include "RmDirMsgEx.h"
#include "MkDirMsgEx.h"


bool MkDirMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "MkDirMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, Log_DEBUG, std::string("Received a MkDirMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   EntryInfo newEntryInfo;

   FhgfsOpsErr mkRes = mkDir(this->getParentInfo(), this->getNewDirName(), &newEntryInfo);

   MkDirRespMsg respMsg(mkRes, &newEntryInfo );
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_MKDIR);

   return true;
}


/**
 * @param dir current directory
 * @param currentDepth 1-based path depth
 */
FhgfsOpsErr MkDirMsgEx::mkDir(EntryInfo* parentInfo, std::string newName,
   EntryInfo* outEntryInfo)
{
   const char* logContext = "MkDirMsg";

   App* app =  Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   TargetCapacityPools* metaCapacityPools = app->getMetaCapacityPools();

   FhgfsOpsErr retVal;

   // reference parent
   DirInode* parentDir = metaStore->referenceDir(parentInfo->getEntryID(), true);
   if(!parentDir)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // check whether localNode owns this (parent) directory
   uint16_t localNodeID = app->getLocalNodeNumID();

   if(parentDir->getOwnerNodeID() != localNodeID)
   { // this node doesn't own the parent dir
      LogContext(logContext).logErr(std::string("Dir-owner mismatch: \"") +
         StringTk::int64ToStr(parentDir->getOwnerNodeID() )  + "\" vs. \"" +
         StringTk::int64ToStr(localNodeID) + "\"");
      metaStore->releaseDir(parentInfo->getEntryID() );
      return FhgfsOpsErr_NOTOWNER;
   }

   // choose new directory owner
   UInt16List preferredNodes;
   UInt16Vector newOwnerNodes;

   parsePreferredNodes(&preferredNodes);

   metaCapacityPools->chooseStorageTargets(1, &preferredNodes, &newOwnerNodes);
   if(newOwnerNodes.empty() )
   { // (might be caused by a bad list of preferred nodes)
      LogContext(logContext).logErr("No metadata server available for new directory: " + newName);

      metaStore->releaseDir(parentInfo->getEntryID() );
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   uint16_t ownerNodeID = newOwnerNodes[0];
   std::string entryID = StorageTk::generateFileID(localNodeID);
   std::string parentEntryID = parentInfo->getEntryID();

   outEntryInfo->update(ownerNodeID, parentEntryID, entryID, newName,
      DirEntryType_DIRECTORY, 0);


   // create remote dir metadata
   // (we create this before the local dir to make sure there is no entry with a colliding name)
   retVal = mkRemoteDirInode(parentDir, newName, outEntryInfo);
   if(likely(retVal == FhgfsOpsErr_SUCCESS) )
   { // remote dir created => create dentry in parent dir

      retVal = mkDirDentry(parentDir, newName, outEntryInfo);
      if(retVal != FhgfsOpsErr_SUCCESS)
      { // error (or maybe name just existed already) => compensate metaDir creation
         mkRemoteDirCompensate(outEntryInfo);
      }
   }

   // clean-up
   metaStore->releaseDir(parentInfo->getEntryID() );

   return retVal;
}

FhgfsOpsErr MkDirMsgEx::mkDirDentry(DirInode* parentDir, std::string& name, EntryInfo* entryInfo)
{
   std::string entryID     = entryInfo->getEntryID();
   uint16_t ownerNodeID    = entryInfo->getOwnerNodeID();

   DirEntry* newDirDentry = new DirEntry(DirEntryType_DIRECTORY, name, entryID, ownerNodeID);

   return parentDir->makeDirEntry(newDirDentry);
}

/**
 * Create dir inode on a remote server.
 *
 * @param name only used for logging
 */
FhgfsOpsErr MkDirMsgEx::mkRemoteDirInode(DirInode* parentDir, std::string& name,
   EntryInfo* entryInfo)
{
   const char* logContext = "MkDirMsg (mk dir inode)";

   App* app = Program::getApp();
   NodeStore* nodes = app->getMetaNodes();

   FhgfsOpsErr mkLocalRes = FhgfsOpsErr_SUCCESS;

   StripePattern* pattern = parentDir->getStripePatternClone();
   uint16_t ownerNodeID = entryInfo->getOwnerNodeID();

   LOG_DEBUG(logContext, Log_DEBUG,
      "Creating dir inode at metadata node: " + StringTk::uintToStr(ownerNodeID) + "; "
      "dirname: " + name);

   uint16_t parentNodeID = app->getLocalNode()->getNumID();
   MkLocalDirMsg mkMsg(entryInfo, getUserID(), getGroupID(), getMode(), pattern, parentNodeID);


   // send request to the new owner node and receive the response message
   do // this loop just exists to enable the "break"-jump (so it's not really a loop)
   {
      Node* node = nodes->referenceNode(ownerNodeID);
      if(!node)
      {
         LogContext(logContext).log(Log_WARNING,
            "Metadata node no longer exists: " + StringTk::uintToStr(ownerNodeID) + "; "
            "dirname: " + name);
         mkLocalRes = FhgfsOpsErr_INTERNAL;
         break;
      }

      // send request to node and receive response
      char* respBuf;
      NetMessage* respMsg;
      bool requestRes = MessagingTk::requestResponse(
         node, &mkMsg, NETMSGTYPE_MkLocalDirResp, &respBuf, &respMsg);

      std::string strOwnerNodeID = node->getNodeIDWithTypeStr();
      nodes->releaseNode(&node);

      if(!requestRes)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with metadata node failed: " + strOwnerNodeID + "; " +
            "dirname: " + name);
         mkLocalRes = FhgfsOpsErr_INTERNAL;
         break;
      }

      // correct response type received
      MkLocalDirRespMsg* mkRespMsg = (MkLocalDirRespMsg*)respMsg;

      FhgfsOpsErr mkLocalErr = (FhgfsOpsErr)mkRespMsg->getValue();
      if(mkLocalErr != FhgfsOpsErr_SUCCESS)
      { // error: local dir not created
         LogContext(logContext).log(Log_WARNING,
            "Metadata node failed to create dir inode: " + strOwnerNodeID + "; " +
            "dirname: " + name);

         delete(respMsg);
         free(respBuf);
         mkLocalRes = FhgfsOpsErr_INTERNAL;
         break;
      }

      // success: local dir created
      LOG_DEBUG(logContext, Log_DEBUG,
         "Node created dir inode: " + strOwnerNodeID + "; " + "dirname: " + name);

      delete(respMsg);
      // cppcheck-suppress doubleFree [special comment to mute false cppcheck alarm]
      free(respBuf);

   } while(0);


   delete(pattern);

   return mkLocalRes;
}

/**
 * Remove dir metadata on a remote server to compensate for creation.
 */
FhgfsOpsErr MkDirMsgEx::mkRemoteDirCompensate(EntryInfo* entryInfo)
{
   LogContext log("MkDirMsg (undo dir inode [" + entryInfo->getFileName() + "])");

   FhgfsOpsErr rmRes = RmDirMsgEx::rmLocalDir(entryInfo);

   if(unlikely(rmRes != FhgfsOpsErr_SUCCESS) )
   { // error
      log.log(Log_WARNING, std::string("Compensation not completely successful. ") +
         "File system might contain (uncritical) inconsistencies.");

      return rmRes;
   }

   log.log(Log_SPAM, "Creation of dir inode compensated");

   return rmRes;
}
