#include <program/Program.h>
#include <common/net/message/storage/moving/MovingDirInsertMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/MetadataTk.h>
#include "RenameV2MsgEx.h"


bool RenameV2MsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("RenameV2Msg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a RenameV2Msg from: ") + peer);

   FhgfsOpsErr renameRes;
   
   EntryInfo* fromDirInfo = this->getFromDirInfo();
   EntryInfo* toDirInfo   = this->getToDirInfo();
   std::string oldName    = this->getOldName();
   std::string newName    = this->getNewName();
   DirEntryType entryType = this->getEntryType();
   
   LOG_DEBUG_CONTEXT(log, 4, "FromID: " + fromDirInfo->getEntryID() + " oldName: '" + oldName + "'"
      + " ToDirID: " + toDirInfo->getEntryID() + " newName: '" + newName + "'");

   renameRes = moveFrom(fromDirInfo, oldName, entryType, toDirInfo, newName);
   
   RenameRespMsg respMsg(renameRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_RENAME);

   return true;      
}

/**
 * Checks existence of the from-part and calls movingPerform().
 */
FhgfsOpsErr RenameV2MsgEx::moveFrom(EntryInfo* fromDirInfo, std::string& oldName,
   DirEntryType entryType, EntryInfo* toDirInfo, std::string& newName)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   
   // reference fromParent
   DirInode* fromParent = metaStore->referenceDir(fromDirInfo->getEntryID(), true);
   if(!fromParent)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr renameRes = movingPerform(fromParent, fromDirInfo, oldName, entryType,
      toDirInfo, newName);

   // clean-up
   metaStore->releaseDir(fromDirInfo->getEntryID() );
   
   return renameRes;
}

FhgfsOpsErr RenameV2MsgEx::movingPerform(DirInode* fromParent, EntryInfo* fromDirInfo,
   std::string& oldName, DirEntryType entryType, EntryInfo* toDirInfo, std::string& newName)
{
   App* app = Program::getApp();
   uint16_t localNodeID = app->getLocalNode()->getNumID();
   
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   
   // is this node the owner of the fromParent dir?
   if(localNodeID != fromParent->getOwnerNodeID() )
      return FhgfsOpsErr_NOTOWNER;

   if (unlikely(entryType == DirEntryType_INVALID) )
   {
      LOG_DEBUG("RenameV2Msg", Log_SPAM, "Received an invalid entry type!");
      return FhgfsOpsErr_INTERNAL;
  }

   if (checkRenameSimple(fromDirInfo, toDirInfo) )
   { // simple rename (<= not a move && everything local)
      LOG_DEBUG("RenameV2Msg::movingPerform", 5, "Method: local rename"); // debug in

      retVal = renameInSameDir(fromParent, oldName, newName);
   }
   else
   if (entryType == DirEntryType_DIRECTORY)
      retVal = this->renameDir(fromParent, fromDirInfo, oldName, toDirInfo, newName);
   else
      retVal = this->renameFile(fromParent, fromDirInfo, oldName, toDirInfo, newName);
   
   return retVal;
}

/**
 * Rename a directory
 */
FhgfsOpsErr RenameV2MsgEx::renameDir(DirInode* fromParent, EntryInfo* fromDirInfo,
   std::string& oldName, EntryInfo* toDirInfo, std::string& newName)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   DirEntry fromDirEntry(oldName);
   bool dirEntryCopyRes = fromParent->getDirDentry(oldName, fromDirEntry);
   if (dirEntryCopyRes == false)
   {
      LOG_DEBUG("RenameV2MsgEx::movingPerform", Log_SPAM, "getDirEntryCopy() failed");
      return FhgfsOpsErr_NOTADIR;
   }

   // when we we verified the 'oldName' is really a directory

#if 0
   // FIXME Bernd: Add local dir-rename/move again, but use the remote functions directly
   App* app = Program::getApp();
   uint16_t localNodeID = app->getLocalNode()->getNumID();

   if(localNodeID == toDirInfo->getOwnerNodeID() )
   { // local dir move (target parent dir is local)
      LOG_DEBUG("RenameV2MsgEx::movingPerform", 5, "Method: local dir move."); // debug in

      retVal = moveLocalDirTo(fromParent, oldName, toDirInfo, newName);

      // TODO trac #124 - possible renameat() race
   }
   else
#endif
   { // remote dir move (target parent dir owned by another node)

      LOG_DEBUG("RenameV2MsgEx::movingPerform", Log_SPAM, "Method: remote dir move."); // debug in

      // prepare local part of the move operation
      char* serialBuf = (char*)malloc(META_SERBUF_SIZE);

      /* Put all meta data of this dentry into the given buffer. The buffer then will be
       * used on the remote side to fill the new dentry */
      size_t usedBufLen = fromDirEntry.serializeDentry(serialBuf);

      { // remote insertion
         retVal = remoteDirInsert(toDirInfo, newName, serialBuf, usedBufLen );


         // finish local part of the move operation
         if(retVal == FhgfsOpsErr_SUCCESS)
         {
            DirEntry* rmDirEntry;

            retVal = fromParent->removeDir(oldName, &rmDirEntry);

            if (retVal == FhgfsOpsErr_SUCCESS)
            {
               std::string parentID = fromParent->getID();
               EntryInfo removedInfo;

               rmDirEntry->getEntryInfo(parentID, 0, &removedInfo);

               MetaStore* metaStore = Program::getApp()->getMetaStore();
               metaStore->setAttr(&removedInfo, 0, NULL); // update ctime of the renamed dir

            }
            else
            {
               // TODO: Now need to remove the remote-dir
            }

            SAFE_DELETE(rmDirEntry);

         }

      }

      // clean up
      free(serialBuf);
   }

   return retVal;
}

/**
 * Rename a file
 */
FhgfsOpsErr RenameV2MsgEx::renameFile(DirInode* fromParent, EntryInfo* fromDirInfo,
   std::string& oldName, EntryInfo* toDirInfo, std::string& newName)
{
   const char* logContext = "RenameV2MsgEx::renameFile";
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   EntryInfo fromFileEntryInfo;
   bool getRes = fromParent->getFileEntryInfo(oldName, fromFileEntryInfo);
   if(getRes == false )
   {
      LOG_DEBUG(logContext, Log_SPAM, "Error: fromDir does not exist.");
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   // when we are here we verified the file to be renamed is really a file


   // the buffer is used to transfer all data of the data of the dir-entry
   char* serialBuf = (char*)malloc(META_SERBUF_SIZE);
   size_t usedSerialBufLen;

   retVal = metaStore->moveRemoteFileBegin(
      fromParent, &fromFileEntryInfo, serialBuf, META_SERBUF_SIZE, &usedSerialBufLen);
   if (retVal != FhgfsOpsErr_SUCCESS)
   {
      free(serialBuf);
      return retVal;
   }

   // FIXME Bernd: call moveRemoteFileInsert() if toDir is on the same server
#if 0
   uint16_t localNodeID = app->getLocalNode()->getNumID();
   if(localNodeID == toDirInfo->getOwnerNodeID() )
   { // local file move (target parent dir is local)

      LOG_DEBUG(logContext, Log_SPAM, "Method: local file move."); // debug in

      retVal = moveLocalFileTo(fromParent, oldName, toDirInfo, newName);
   }
   else
#endif
   { // remote file move (target parent dir owned by another node)

      LOG_DEBUG(logContext, Log_SPAM, "Method: remote file move."); // debug in


      // Do the remote operation (insert and possible unlink of an existing toFile)
      retVal = remoteFileInsertAndUnlink(&fromFileEntryInfo, toDirInfo, newName, serialBuf,
         usedSerialBufLen);


      metaStore->moveRemoteFileComplete(fromParent, fromFileEntryInfo.getEntryID() );

      // finish local part of the owned file move operation (+ remove local file-link)
      if(retVal == FhgfsOpsErr_SUCCESS)
      {
         // We are not interested in the inode here , as we are not going to delete storage chunks.
         FhgfsOpsErr unlinkRes = metaStore->unlinkFile(fromParent->getID(), oldName, NULL);
         if (unlikely (unlinkRes && unlinkRes != FhgfsOpsErr_PATHNOTEXISTS ) )
         {
            /* Hmm, what now? If the user now deletes either of both files, the other
             * will get invalid as the chunks get lost.
             * TODO: We probably need to delete the just-successfully-created-toFile */
            LogContext(logContext).logErr(std::string("Error: Failed to unlink fromFile: ") +
               "DirID: " + fromFileEntryInfo.getEntryID() +" entryName: " + oldName + ". " +
               "Remote toFile was successfully created. Better run fsck now.");
         }
      }

      // clean up
      free(serialBuf);
   }

   return retVal;
}


/**
 * Checks whether the requested rename operation is not actually a move operation, but really
 * just a simple rename (in which case nothing is moved to a new directory)
 * 
 * @return true if it is a simple rename (no moving involved)
 */ 
bool RenameV2MsgEx::checkRenameSimple(EntryInfo* fromDirInfo, EntryInfo* toDirInfo)
{
   if (fromDirInfo->getEntryID() == toDirInfo->getEntryID() )
      return true;

   return false;
}

/**
 * Renames a directory or a file (no moving between different directories involved).
 */
FhgfsOpsErr RenameV2MsgEx::renameInSameDir(DirInode* fromParent, std::string oldName,
   std::string toName)
{
   const char* logContext = "Rename";
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   
   /* we are passing here the very same fromParent pointer also a toParent pointer, which is
    * essential in order not to dead-lock */

   FileInode* unlinkInode; // inode belong to a possibly existing toName file

   FhgfsOpsErr renameRes = metaStore->renameFileInSameDir(fromParent, oldName, toName,
      &unlinkInode);

   if (renameRes == FhgfsOpsErr_SUCCESS && unlinkInode)
   {
      std::string entryID = unlinkInode->getEntryID();

      FhgfsOpsErr chunkUnlinkRes = MsgHelperUnlink::unlinkChunkFiles(unlinkInode);
      if (chunkUnlinkRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).logErr(std::string("Rename succeeded, but unlinking storage ") +
         "chunk files of the overwritten targetFileName (" + toName + ") failed. " +
         "Entry-ID: " + entryID);

         // we can't do anything about it, so we won't even inform the user
      }
   }


   return renameRes;
}

/**
 * Note: This method not only sends the insertion message, but also unlinks an overwritten local
 * file if it is contained in the response.
 *
 * @param serialBuf   the inode values serialized into this buffer.
 */
FhgfsOpsErr RenameV2MsgEx::remoteFileInsertAndUnlink(EntryInfo* fromFileInfo, EntryInfo* toDirInfo,
   std::string newName, char* serialBuf, size_t serialBufLen)
{
   LogContext log("RenameV2MsgEx::remoteFileInsert");
   
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   
   NodeStore* nodes = Program::getApp()->getMetaNodes();

   uint16_t toNodeID = toDirInfo->getOwnerNodeID();

   LOG_DEBUG_CONTEXT(log, Log_DEBUG,
      "Inserting remote parentID: " + toDirInfo->getEntryID() + "; "
      "newName: '" + newName + "'; "
      "node: " + StringTk::uintToStr(toNodeID) );

   MovingFileInsertMsg insertMsg(fromFileInfo, toDirInfo, newName, serialBuf, serialBufLen);
   
   // send request to the owner node and receive the response message
   Node* node = nodes->referenceNode(toDirInfo->getOwnerNodeID() );
   if(!node)
   {
      log.log(Log_WARNING, "Metadata Node no longer exists: NodeID: " + StringTk::uintToStr(toNodeID) );
      return FhgfsOpsErr_INTERNAL;
   }

   // send request to node and receive response
   char* respBuf;
   NetMessage* respMsg;
   bool requestRes = MessagingTk::requestResponse(
      node, &insertMsg, NETMSGTYPE_MovingFileInsertResp, &respBuf, &respMsg);

   nodes->releaseNode(&node);

   if(!requestRes)
   { // communication error
      log.log(Log_WARNING,
         "Communication with metadata node failed: NodeID: " + StringTk::uintToStr(toNodeID) );
      return FhgfsOpsErr_INTERNAL;
   }

   // correct response type received
   MovingFileInsertRespMsg* insertRespMsg = (MovingFileInsertRespMsg*)respMsg;
   
   retVal = (FhgfsOpsErr)insertRespMsg->getResult();
   
   std::string unlinkedFileID = insertRespMsg->getUnlinkedFileID();
   if(unlinkedFileID.length() )
   { // a file was overwritten => unlink local file
      // note: we do not notify the caller about unlink errors, because the rename
      // itself might still work
      StripePattern* unlinkedFilePattern = insertRespMsg->createPattern();
      unsigned chunkHash = insertRespMsg->getChunkHash();
      
      // some fake values, we just want to delete the file
      int mode = 0;
      unsigned userID = 0;
      unsigned groupID = 0;
      DirEntryType entryType = DirEntryType_REGULARFILE;
      unsigned featureFlags = 0;

      FileInode* toUnlinkInode =  new FileInode(unlinkedFileID, mode, userID, groupID,
         *unlinkedFilePattern, entryType, featureFlags, chunkHash);

      MsgHelperUnlink::unlinkChunkFiles(toUnlinkInode); // destructs toUnlinkInode
      
      delete(unlinkedFilePattern);
   }

   if(retVal != FhgfsOpsErr_SUCCESS)
   { // error: remote file not inserted
      LOG_DEBUG_CONTEXT(log, Log_NOTICE,
         "Metadata node was unable to insert file: NodeID: " + StringTk::uintToStr(toNodeID) +
         "Error: " + FhgfsOpsErrTk::toErrString(retVal) );
   }
   else
   {
      // success: local dir created
      LOG_DEBUG_CONTEXT(log, Log_DEBUG,
         "Metadata node inserted file: NodeID: " + StringTk::uintToStr(toNodeID) );
   }


   delete(respMsg);
   free(respBuf);

   return retVal;
}

/**
 * Insert a remote directory dir-entry
 */
FhgfsOpsErr RenameV2MsgEx::remoteDirInsert(EntryInfo* toDirInfo, std::string& newName,
   char* serialBuf, size_t serialBufLen)
{
   LogContext log("RenameV2MsgEx::remoteDirInsert");
   
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   
   NodeStore* nodes = Program::getApp()->getMetaNodes();
   
   uint16_t toNodeID = toDirInfo->getOwnerNodeID();

   LOG_DEBUG_CONTEXT(log, Log_DEBUG,
      "Inserting remote parentID: " + toDirInfo->getEntryID() + "; "
      "newName: '" + newName + "'; "
      "node: " + StringTk::uintToStr(toNodeID) );

   MovingDirInsertMsg insertMsg(toDirInfo, newName, serialBuf, serialBufLen);
   
   // send request to the owner node and receive the response message

   Node* node = nodes->referenceNode(toDirInfo->getOwnerNodeID() );
   if(!node)
   {
      log.log(Log_WARNING, "Metadata node no longer exists: NodeID: " + StringTk::uintToStr(toNodeID) );
      return FhgfsOpsErr_INTERNAL;
   }

   // send request to node and receive response
   char* respBuf;
   NetMessage* respMsg;
   bool requestRes = MessagingTk::requestResponse(
      node, &insertMsg, NETMSGTYPE_MovingDirInsertResp, &respBuf, &respMsg);

   nodes->releaseNode(&node);

   if(!requestRes)
   { // communication error
      log.log(Log_WARNING,
         "Communication with metadata node failed: NodeID: " + StringTk::uintToStr(toNodeID) );
      return FhgfsOpsErr_INTERNAL;
   }

   // correct response type received
   MovingDirInsertRespMsg* insertRespMsg = (MovingDirInsertRespMsg*)respMsg;

   retVal = (FhgfsOpsErr)insertRespMsg->getValue();
   if(retVal != FhgfsOpsErr_SUCCESS)
   { // error: remote file not inserted
      LOG_DEBUG_CONTEXT(log, Log_NOTICE,
         "Metdata node was unable to insert directory: Node-ID: " + StringTk::uintToStr(toNodeID) +
         " Error: " + FhgfsOpsErrTk::toErrString(retVal) );
   }
   else
   {
      // success: local dir created
      LOG_DEBUG_CONTEXT(log, Log_DEBUG,
         "Metadata node inserted local directory: NodeID: " + StringTk::uintToStr(toNodeID) );
   }

   delete(respMsg);
   free(respBuf);

   
   return retVal;
}

