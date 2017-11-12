#include "LinkToLostAndFoundMsgEx.h"

#include <common/net/message/fsck/RemoveInodeMsg.h>
#include <common/net/message/fsck/RemoveInodeRespMsg.h>
#include <program/Program.h>

bool LinkToLostAndFoundMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG("LinkToLostAndFoundMsg incoming", 4,
      std::string("Received a LinkToLostAndFoundMsg from: ") + peer);

   if (FsckDirEntryType_ISDIR(this->getEntryType()))
   {
      FsckDirEntryList createdDirEntries;
      FsckDirInodeList failedInodes;
      linkDirInodes(&failedInodes, &createdDirEntries);
      LinkToLostAndFoundRespMsg respMsg(&failedInodes, &createdDirEntries);

      respMsg.serialize(respBuf, bufLen);
      sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*)fromAddr,
         sizeof(struct sockaddr_in) );
   }
   /*
    * TODO: is it still important to have L+F for file inodes or is it impossible to happen, that a
    * file inode without dentry exists
   else
   {
      FsckDirEntryList createdDirEntries;
      FsckFileInodeList failedInodes;
      linkFileInodes(&failedInodes, &createdDirEntries);
      LinkToLostAndFoundRespMsg respMsg(&failedInodes, &createdDirEntries);

      respMsg.serialize(respBuf, bufLen);
      sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*)fromAddr,
         sizeof(struct sockaddr_in) );
   }

*/

   return true;
}

void LinkToLostAndFoundMsgEx::linkDirInodes(FsckDirInodeList* outFailedInodes,
   FsckDirEntryList* outCreatedDirEntries)
{
   const char* logContext = "LinkToLostAndFoundMsgEx (linkDirInodes)";

   uint16_t localNodeNumID = Program::getApp()->getLocalNode()->getNumID();

   FsckDirInodeList dirInodes;
   this->parseInodes(&dirInodes);

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryInfo* lostAndFoundInfo = this->getLostAndFoundInfo();
   DirInode *lostAndFoundDir = metaStore->referenceDir(lostAndFoundInfo->getEntryID(), true);

   if ( !lostAndFoundDir )
   {
      *outFailedInodes = dirInodes;
      return;
   }
   else
   {
      for ( FsckDirInodeListIter iter = dirInodes.begin(); iter != dirInodes.end(); iter++ )
      {
         std::string entryID     = iter->getID();
         uint16_t ownerNodeID    = iter->getOwnerNodeID();
         DirEntryType entryType  = DirEntryType_DIRECTORY;
         DirEntry* newDirEntry = new DirEntry(entryType, entryID, entryID, ownerNodeID);

         bool makeRes = lostAndFoundDir->makeDirEntry(newDirEntry);

         // stat the new file to get device and inode information
         std::string filename = MetaStorageTk::getMetaDirEntryPath(
            Program::getApp()->getStructurePath()->getPathAsStrConst(),
            lostAndFoundInfo->getEntryID()) + "/" + entryID;

         struct stat statBuf;

         int statRes = stat(filename.c_str(), &statBuf);

         int saveDevice;
         uint64_t saveInode;
         if ( likely(!statRes) )
         {
            saveDevice = statBuf.st_dev;
            saveInode = statBuf.st_ino;
         }
         else
         {
            saveDevice = 0;
            saveInode = 0;
            LogContext(logContext).log(Log_CRITICAL,
               "Could not stat dir entry file; entryID: " + entryID + ";filename: " + filename);
         }

         if ( makeRes != FhgfsOpsErr_SUCCESS )
            outFailedInodes->push_back(*iter);
         else
         {
            std::string parentID = lostAndFoundInfo->getEntryID();
            FsckDirEntry newFsckDirEntry(entryID, entryID, parentID,
               localNodeNumID, ownerNodeID, FsckDirEntryType_DIRECTORY, localNodeNumID, saveDevice,
               saveInode);
            outCreatedDirEntries->push_back(newFsckDirEntry);
         }
      }

      lostAndFoundDir->refreshMetaInfo();
      metaStore->releaseDir(lostAndFoundInfo->getEntryID() );
   }
}

/*
 * TODO: is it still important to have L+F for file inodes or is it impossible to happen, that a
 * file inode without dentry exists

void LinkToLostAndFoundMsgEx::linkFileInodes(FsckFileInodeList* outFailedInodes,
   FsckDirEntryList* outCreatedDirEntries)
{
   const char* logContext = "LinkToLostAndFoundMsgEx (linkFileInodes)";

   uint16_t localNodeNumID = Program::getApp()->getLocalNode()->getNumID();

   FsckFileInodeList fileInodes;
   this->parseInodes(&fileInodes);

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryInfo* lostAndFoundInfo = this->getLostAndFoundInfo();
   DirInode *lostAndFoundDir = metaStore->referenceDir(lostAndFoundInfo->getEntryID(), true);

   if ( !lostAndFoundDir )
   {
      *outFailedInodes = fileInodes;
      return;
   }
   else
   {
      for ( FsckFileInodeListIter iter = fileInodes.begin(); iter != fileInodes.end(); iter++ )
      {
         std::string entryID = iter->getID();
         uint16_t oldInodeOwner = iter->getSaveNodeID();

         if (localNodeNumID != iter->getSaveNodeID())
         {
            // the file inode is not on this metadata server => we need to create it here, because
            // we want the file inode to be on the same node as the dir entry
            int mode = iter->getMode();
            unsigned userID = iter->getUserID();
            unsigned groupID = iter->getGroupID();

            StripePattern* stripePattern = FsckTk::FsckStripePatternToStripePattern(
               iter->getStripeTargets(), iter->getStripePatternType());

            FileInode* newInode = new FileInode(entryID, mode, userID, groupID, *stripePattern );

            delete(stripePattern);

            FhgfsOpsErr insertRes = metaStore->makeFileInode(newInode);

            if (insertRes != FhgfsOpsErr_SUCCESS)
            {
               delete (newInode);
               outFailedInodes->push_back(*iter);
               continue;
            }

            // worked fine => now we need to delete the inode on the old node
            {
               FhgfsOpsErr rmRes = this->deleteInode(entryID, oldInodeOwner);
               if (rmRes != FhgfsOpsErr_SUCCESS)
               {
                  // could not delete old inode => TODO : What are we doing now??
                  // delete the new inode and abort?
                  LogContext(logContext).logErr("Could not delete inode on node; entryID: " +
                     entryID + " nodeID: " + StringTk::uintToStr(oldInodeOwner));
               }
            }
         }

         // create the actual entry
         DirEntryType entryType = DirEntryType_REGULARFILE;

         DirEntry* newDirEntry = new DirEntry(entryType, entryID, entryID, localNodeNumID);

         bool makeRes = lostAndFoundDir->makeDirEntry(newDirEntry);

         if ( makeRes != FhgfsOpsErr_SUCCESS )
         {
            outFailedInodes->push_back(*iter);
            continue;
         }

         std::string parentID = lostAndFoundInfo->getEntryID();
         FsckDirEntry newFsckDirEntry(entryID, entryID, parentID, localNodeNumID, localNodeNumID,
            FsckDirEntryType_REGULARFILE, localNodeNumID);
         outCreatedDirEntries->push_back(newFsckDirEntry);
      }

      lostAndFoundDir->refreshMetaInfo();
      metaStore->releaseDir(lostAndFoundDir);
   }
}

FhgfsOpsErr LinkToLostAndFoundMsgEx::deleteInode(std::string& entryID, uint16_t ownerNodeID)
{
   const char* logContext = "LinkToLostAndFoundMsgEx (deleteInode)";
   FhgfsOpsErr rmRes =  FhgfsOpsErr_INTERNAL;

   NodeStore* metaNodes = Program::getApp()->getMetaNodes();
   Node* oldOwnerNode = metaNodes->referenceNode(ownerNodeID);

   if (! oldOwnerNode)
   {
      LogContext(logContext).logErr("Old owner node of inode is unknown: " + ownerNodeID);
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   bool commRes;
   char *respBuf = NULL;
   NetMessage *respMsg = NULL;

   RemoveInodeMsg removeInodeMsg(entryID, DirEntryType_REGULARFILE);
   commRes = MessagingTk::requestResponse(oldOwnerNode, &removeInodeMsg,
         NETMSGTYPE_RemoveInodeResp, &respBuf, &respMsg);

   if ( commRes )
   {
      RemoveInodeRespMsg* removeInodeRespMsg = (RemoveInodeRespMsg*) respMsg;
      rmRes = removeInodeRespMsg->getResult();
   }
   else
   {
      LogContext(logContext).logErr("Communication failed with node: " +
         oldOwnerNode->getID());

      rmRes = FhgfsOpsErr_COMMUNICATION;
   }

   SAFE_FREE(respBuf);
   SAFE_DELETE(respMsg);
   metaNodes->releaseNode(&oldOwnerNode);
   return rmRes;
}

*/
