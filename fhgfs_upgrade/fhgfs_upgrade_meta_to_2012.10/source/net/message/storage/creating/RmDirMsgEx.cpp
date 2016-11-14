#include <program/Program.h>
#include <common/net/message/storage/creating/RmDirRespMsg.h>
#include <common/net/message/storage/creating/RmLocalDirMsg.h>
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include "RmDirMsgEx.h"


bool RmDirMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "RmDirMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a RmDirMsg from: ") + peer);
   #endif // FHGFS_DEBUG
   

   FhgfsOpsErr rmRes = this->rmDir();
   
   RmDirRespMsg respMsg(rmRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_RMDIR);

   return true;
}

FhgfsOpsErr RmDirMsgEx::rmDir()
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   
   FhgfsOpsErr retVal;

   EntryInfo* parentInfo = this->getParentInfo();
   std::string delDirName = this->getDelDirName();

   // reference parent
   DirInode* parentDir = metaStore->referenceDir(parentInfo->getEntryID(), true);
   if(!parentDir)
      return FhgfsOpsErr_PATHNOTEXISTS;
   
   DirEntry removeDirEntry(delDirName);
   bool getEntryRes = parentDir->getDirDentry(delDirName, removeDirEntry);
   if(!getEntryRes)
      retVal = FhgfsOpsErr_PATHNOTEXISTS;
   else
   {
      EntryInfo delEntryInfo;
      int additionalFlags = 0;

      std::string parentEntryID = parentInfo->getEntryID();
      removeDirEntry.getEntryInfo(parentEntryID, additionalFlags, &delEntryInfo);

      // remove the remote dir inode first (to make sure it's empty and not in use)
      retVal = rmLocalDir(&delEntryInfo);
      
      if(retVal == FhgfsOpsErr_SUCCESS)
      { // local removal succeeded => remove meta dir
         retVal = parentDir->removeDir(delDirName, NULL);
      }
   }

   // clean-up
   metaStore->releaseDir(parentInfo->getEntryID() );
   
   return retVal;
}

/**
 * Remove the inode of this directory
 */
FhgfsOpsErr RmDirMsgEx::rmLocalDir(EntryInfo* delEntryInfo)
{
   const char* logContext = "RmDirMsg (rm dir inode)";
   
   FhgfsOpsErr rmLocalRes = FhgfsOpsErr_SUCCESS;
   
   NodeStore* nodes = Program::getApp()->getMetaNodes();
   uint16_t nodeID = delEntryInfo->getOwnerNodeID();
   
   LOG_DEBUG(logContext, Log_DEBUG,
      "Removing dir inode from metadata node: " + StringTk::uintToStr(nodeID) + "; "
      "dirname: " + delEntryInfo->getFileName() );

   RmLocalDirMsg rmMsg(delEntryInfo);
   
   // send request to the owner node and receive the response message
   do // this loop just exists to enable the "break"-jump (so it's not really a loop)
   {
      Node* node = nodes->referenceNode(nodeID);
      if(!node)
      {
         LogContext(logContext).log(Log_WARNING,
            "Metadata node no longer exists: "
            "NodeID: " + StringTk::uintToStr(nodeID)      + "; "
            "dirID: " + delEntryInfo->getEntryID()        + "; " +
            "dirname: " + delEntryInfo->getFileName() );
         rmLocalRes = FhgfsOpsErr_INTERNAL;
         break;
      }
      
      // send request to node and receive response
      char* respBuf;
      NetMessage* respMsg;
      bool requestRes = MessagingTk::requestResponse(
         node, &rmMsg, NETMSGTYPE_RmLocalDirResp, &respBuf, &respMsg);
   
      nodes->releaseNode(&node);
   
      if(!requestRes)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with metadata node failed: "
            "NodeID: " + StringTk::uintToStr(nodeID)      + "; "
            "dirID: " + delEntryInfo->getEntryID()        + "; " +
            "dirname: " + delEntryInfo->getFileName() );
         rmLocalRes = FhgfsOpsErr_INTERNAL;
         break;
      }
      
      // correct response type received
      RmLocalDirRespMsg* rmRespMsg = (RmLocalDirRespMsg*)respMsg;
      
      rmLocalRes = (FhgfsOpsErr)rmRespMsg->getValue();
      if(rmLocalRes != FhgfsOpsErr_SUCCESS)
      { // error: local dir not removed
         std::string errString = FhgfsOpsErrTk::toErrString(rmLocalRes);

         LogContext(logContext).log(Log_DEBUG,
            "Metadata node was unable to remove dir inode: "
            "nodeID: "  + StringTk::uintToStr(nodeID)   + "; "
            "dirID: "   + delEntryInfo->getEntryID()    + "; " +
            "dirname: " + delEntryInfo->getFileName()   + "; " +
            "error: "   + errString);
         
         delete(respMsg);
         free(respBuf);
         break;
      }
      
      // success: local dir created
      LOG_DEBUG(logContext, Log_DEBUG,
         "Metadata node removed local directory: "
         "nodeID: " + StringTk::uintToStr(nodeID)      + "; "
         "dirID: " + delEntryInfo->getEntryID()        + "; " +
         "dirname: " + delEntryInfo->getFileName() );
      
      delete(respMsg);
      // cppcheck-suppress doubleFree [special comment to mute false cppcheck alarm]
      free(respBuf);
      
   } while(0);

   
   return rmLocalRes;
}

