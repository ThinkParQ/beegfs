#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/UpdateBacklinkMsg.h>
#include <common/net/message/storage/attribs/UpdateBacklinkRespMsg.h>
#include <common/net/message/storage/attribs/UpdateDirParentMsg.h>
#include <common/net/message/storage/attribs/UpdateDirParentRespMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/MetadataTk.h>
#include <components/ModificationEventFlusher.h>
#include <net/msghelpers/MsgHelperChunkBacklinks.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <program/Program.h>
#include <session/EntryLock.h>
#include "RenameV2MsgEx.h"

#include <boost/scoped_array.hpp>

std::tuple<DirIDLock, DirIDLock, ParentNameLock, ParentNameLock, FileIDLock> RenameV2MsgEx::lock(
   EntryLockStore& store)
{
   ParentNameLock fromNameLock;
   ParentNameLock toNameLock;
   DirIDLock fromDirLock;
   DirIDLock toDirLock;

   // we have to lock the file as well, because concurrent modifications of file attributes may
   // race with the moving operation between two servers.
   FileIDLock fileLock;

   EntryInfo fromFileInfo;

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   DirInode* fromDir = metaStore->referenceDir(getFromDirInfo()->getEntryID(),
         getFromDirInfo()->getIsBuddyMirrored(), true);
   if (!fromDir)
      return {};
   else
   {
      fromDir->getFileEntryInfo(getOldName(), fromFileInfo);
      metaStore->releaseDir(getFromDirInfo()->getEntryID());
   }

   // take care about lock ordering! see MirroredMessage::lock()
   // since directories are locked for read, and by the same id as the (parent,name) tuples,the
   // same ordering applies.
   if (getFromDirInfo()->getEntryID() < getToDirInfo()->getEntryID()
         || (getFromDirInfo()->getEntryID() == getToDirInfo()->getEntryID()
               && getOldName() < getNewName()))
   {
      fromDirLock = {&store, getFromDirInfo()->getEntryID(), true};
      if (getFromDirInfo()->getEntryID() != getToDirInfo()->getEntryID())
         toDirLock = {&store, getToDirInfo()->getEntryID(), true};

      fromNameLock = {&store, getFromDirInfo()->getEntryID(), getOldName()};
      if (getOldName() != getNewName())
         toNameLock = {&store, getToDirInfo()->getEntryID(), getNewName()};
   }
   else
   {
      toDirLock = {&store, getToDirInfo()->getEntryID(), true};
      if (getFromDirInfo()->getEntryID() != getToDirInfo()->getEntryID())
         fromDirLock = {&store, getFromDirInfo()->getEntryID(), true};

      toNameLock = {&store, getToDirInfo()->getEntryID(), getNewName()};
      if (getOldName() != getNewName())
         fromNameLock = {&store, getFromDirInfo()->getEntryID(), getOldName()};
   }

   if (DirEntryType_ISFILE(fromFileInfo.getEntryType()) && fromFileInfo.getIsInlined())
      fileLock = {&store, fromFileInfo.getEntryID()};

   return std::make_tuple(
         std::move(fromDirLock), std::move(toDirLock),
         std::move(fromNameLock), std::move(toNameLock),
         std::move(fileLock));
}

bool RenameV2MsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "RenameV2Msg incoming";

   LOG_DEBUG(logContext, Log_DEBUG, "Received a RenameV2Msg from: " + ctx.peerName() );
   IGNORE_UNUSED_VARIABLE(logContext);

   LOG_DEBUG(logContext, Log_DEBUG, "FromDirID: " + getFromDirInfo()->getEntryID() + "; "
      "oldName: '" + getOldName() + "'; "
      "ToDirID: " + getToDirInfo()->getEntryID() + "; "
      "newName: '" + getNewName() + "'");

   BaseType::processIncoming(ctx);

   // update operation counters
   Program::getApp()->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      MetaOpCounter_RENAME, getMsgHeaderUserID() );

   return true;
}

/**
 * Checks existence of the from-part and calls movingPerform().
 */
std::unique_ptr<MirroredMessageResponseState> RenameV2MsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   ModificationEventFlusher* modEventFlusher = Program::getApp()->getModificationEventFlusher();
   bool modEventLoggingEnabled = modEventFlusher->isLoggingEnabled();

   std::string movedEntryID;
   std::string unlinkedEntryID;

   // reference fromParent
   DirInode* fromParent = metaStore->referenceDir(getFromDirInfo()->getEntryID(),
      getFromDirInfo()->getIsBuddyMirrored(), true);
   if(!fromParent)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   if (modEventLoggingEnabled)
   {
      EntryInfo entryInfo;
      fromParent->getEntryInfo(getOldName(), entryInfo);

      movedEntryID = entryInfo.getEntryID();
   }

   FhgfsOpsErr renameRes = movingPerform(*fromParent, getFromDirInfo(), getOldName(),
         getEntryType(), getToDirInfo(), getNewName(), unlinkedEntryID);

   if (shouldFixTimestamps())
      fixInodeTimestamp(*fromParent, fromDirTimestamps);

   metaStore->releaseDir(getFromDirInfo()->getEntryID());

   // response early because event logging may block
   earlyComplete(ctx, ResponseState(renameRes));

   // clean-up

   if (modEventLoggingEnabled)
   {
      if (DirEntryType_ISDIR(getEntryType()))
         modEventFlusher->add(ModificationEvent_DIRMOVED, movedEntryID);
      else
      {
         modEventFlusher->add(ModificationEvent_FILEMOVED, movedEntryID);
         if (!unlinkedEntryID.empty())
            modEventFlusher->add(ModificationEvent_FILEREMOVED, unlinkedEntryID);
      }
   }

   return {};
}

FhgfsOpsErr RenameV2MsgEx::movingPerform(DirInode& fromParent, EntryInfo* fromDirInfo,
   const std::string& oldName, DirEntryType entryType, EntryInfo* toDirInfo,
   const std::string& newName, std::string& unlinkedEntryID)
{
   const char* logContext = "RenameV2MsgEx::movingPerform";
   IGNORE_UNUSED_VARIABLE(logContext);

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   // is this node the owner of the fromParent dir?
   NumNodeID expectedOwnerID = fromParent.getIsBuddyMirrored() ?
      NumNodeID(metaBuddyGroupMapper->getLocalGroupID() ) : app->getLocalNode().getNumID();

   if (fromParent.getOwnerNodeID() != expectedOwnerID)
      return FhgfsOpsErr_NOTOWNER;

   if (unlikely(entryType == DirEntryType_INVALID) )
   {
      LOG_DEBUG(logContext, Log_SPAM, "Received an invalid entry type!");
      return FhgfsOpsErr_INTERNAL;
  }

   if (fromDirInfo->getEntryID() == toDirInfo->getEntryID())
   { // simple rename (<= not a move && everything local)
      LOG_DEBUG(logContext, Log_SPAM, "Method: rename in same dir"); // debug in

      retVal = renameInSameDir(fromParent, oldName, newName, unlinkedEntryID);
   }
   else
   if (entryType == DirEntryType_DIRECTORY)
      retVal = renameDir(fromParent, fromDirInfo, oldName, toDirInfo, newName);
   else
      retVal = renameFile(fromParent, fromDirInfo, oldName, toDirInfo, newName,
         unlinkedEntryID);

   return retVal;
}

bool RenameV2MsgEx::forwardToSecondary(ResponseContext& ctx)
{
   return sendToSecondary(ctx, *this, NETMSGTYPE_RenameResp) == FhgfsOpsErr_SUCCESS;
}

/**
 * Rename a directory
 */
FhgfsOpsErr RenameV2MsgEx::renameDir(DirInode& fromParent, EntryInfo* fromDirInfo,
   const std::string& oldName, EntryInfo* toDirInfo, const std::string& newName)
{
   const char* logContext = "RenameV2MsgEx::renameDir";

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   DirEntry fromDirEntry(oldName);
   bool dirEntryCopyRes = fromParent.getDirDentry(oldName, fromDirEntry);
   if (dirEntryCopyRes == false)
   {
      LOG_DEBUG("RenameV2MsgEx::movingPerform", Log_SPAM, "getDirEntryCopy() failed");
      return FhgfsOpsErr_NOTADIR;
   }

   // when we we verified the 'oldName' is really a directory

   LOG_DEBUG(logContext, Log_SPAM, "Method: remote dir move."); // debug in

   if (fromDirInfo->getIsBuddyMirrored() && hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
   {
      retVal = FhgfsOpsErr_SUCCESS;
   }
   else
   {
      // prepare local part of the move operation
      boost::scoped_array<char> serialBuf(new char[META_SERBUF_SIZE]);
      Serializer ser(serialBuf.get(), META_SERBUF_SIZE);

      /* Put all meta data of this dentry into the given buffer. The buffer then will be
       * used on the remote side to fill the new dentry */
      fromDirEntry.serializeDentry(ser);

      if (!ser.good())
         LogContext(logContext).logErr("dentry too large: " + oldName);
      else
         retVal = remoteDirInsert(toDirInfo, newName, serialBuf.get(), ser.size());
   }

   // finish local part of the move operation
   if(retVal == FhgfsOpsErr_SUCCESS)
   {
      DirEntry* rmDirEntry;

      retVal = fromParent.removeDir(oldName, &rmDirEntry);

      if (retVal == FhgfsOpsErr_SUCCESS)
      {
         std::string parentID = fromParent.getID();
         EntryInfo removedInfo;

         rmDirEntry->getEntryInfo(parentID, 0, &removedInfo);

         updateRenamedDirInode(&removedInfo, toDirInfo);
      }
      else
      {
         // TODO: We now need to undo the remote DirInsert.

         LogContext(logContext).log(Log_CRITICAL,
            std::string("Failed to remove fromDir: ") + oldName +
            std::string(" Error: ") + FhgfsOpsErrTk::toErrString(retVal) );
      }

      SAFE_DELETE(rmDirEntry);
   }

   return retVal;
}

/**
 * Rename a file
 */
FhgfsOpsErr RenameV2MsgEx::renameFile(DirInode& fromParent, EntryInfo* fromDirInfo,
   const std::string& oldName, EntryInfo* toDirInfo, const std::string& newName,
   std::string& unlinkedEntryID)
{
   const char* logContext = "RenameV2MsgEx::renameFile";
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   EntryInfo fromFileEntryInfo;
   bool getRes = fromParent.getFileEntryInfo(oldName, fromFileEntryInfo);
   if(getRes == false )
   {
      LOG_DEBUG(logContext, Log_SPAM, "Error: fromDir does not exist.");
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   // when we are here we verified the file to be renamed is really a file

   if (fromDirInfo->getIsBuddyMirrored() && hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
   {
      retVal = FhgfsOpsErr_SUCCESS;
   }
   else
   {
      // the buffer is used to transfer all data of the data of the dir-entry
      boost::scoped_array<char> serialBuf(new char[META_SERBUF_SIZE]);
      size_t usedSerialBufLen;
      StringVector xattrNames;

      retVal = metaStore->moveRemoteFileBegin(
         fromParent, &fromFileEntryInfo, serialBuf.get(), META_SERBUF_SIZE, &usedSerialBufLen);
      if (retVal != FhgfsOpsErr_SUCCESS)
         return retVal;

      // TODO: call moveRemoteFileInsert() if toDir is on the same server
      LOG_DEBUG(logContext, Log_SPAM, "Method: remote file move."); // debug in

      if (app->getConfig()->getStoreClientXAttrs())
      {
         FhgfsOpsErr listXAttrRes;

         std::tie(listXAttrRes, xattrNames) = MsgHelperXAttr::listxattr(&fromFileEntryInfo);
         if (listXAttrRes != FhgfsOpsErr_SUCCESS)
         {
            metaStore->moveRemoteFileComplete(fromParent, fromFileEntryInfo.getEntryID());
            return FhgfsOpsErr_TOOBIG;
         }
      }

      // Do the remote operation (insert and possible unlink of an existing toFile)
      retVal = remoteFileInsertAndUnlink(&fromFileEntryInfo, toDirInfo, newName, serialBuf.get(),
         usedSerialBufLen, std::move(xattrNames), unlinkedEntryID);
   }


   // finish local part of the owned file move operation (+ remove local file-link)
   if(retVal == FhgfsOpsErr_SUCCESS)
   {
      // We are not interested in the inode here , as we are not going to delete storage chunks.
      EntryInfo entryInfo;
      FhgfsOpsErr unlinkRes = metaStore->unlinkFile(fromParent, oldName, &entryInfo, NULL);

      if (unlikely (unlinkRes) )
      {
         /* TODO: We now need to undo the remote FileInsert. If the user removes either
          *       entry herself, the other entry will be invalid, as the chunk files already
          *       will be removed on the first unlink. */

            LogContext(logContext).logErr(std::string("Error: Failed to unlink fromFile: ") +
               "DirID: " + fromFileEntryInfo.getParentEntryID() + " "
               "entryName: " + oldName + ". " +
               "Remote toFile was successfully created.");
      }
   }

   if (!fromDirInfo->getIsBuddyMirrored() || !hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
      metaStore->moveRemoteFileComplete(fromParent, fromFileEntryInfo.getEntryID() );

   return retVal;
}

/**
 * Renames a directory or a file (no moving between different directories involved).
 *
 * Note: We do not need to update chunk backlinks here, as nothing relevant to an
 *       for those will be modified here.
 */
FhgfsOpsErr RenameV2MsgEx::renameInSameDir(DirInode& fromParent, const std::string& oldName,
   const std::string& toName, std::string& unlinkedEntryID)
{
   const char* logContext = "RenameV2MsgEx::renameInSameDir";
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   /* we are passing here the very same fromParent pointer also a toParent pointer, which is
    * essential in order not to dead-lock */

   std::unique_ptr<FileInode> unlinkInode; // inode belong to a possibly existing toName file

   FhgfsOpsErr renameRes = metaStore->renameInSameDir(fromParent, oldName, toName,
         &unlinkInode);

   if (renameRes == FhgfsOpsErr_SUCCESS)
   {
      if (shouldFixTimestamps())
      {
         DirEntry dentry(toName);
         if (fromParent.getDentry(toName, dentry))
         {
            EntryInfo info;
            dentry.getEntryInfo(fromParent.getID(), 0, &info);

            if (DirEntryType_ISDIR(info.getEntryType()))
            {
               auto dir = metaStore->referenceDir(info.getEntryID(), info.getIsBuddyMirrored(),
                     true);
               if (dir)
               {
                  fixInodeTimestamp(*dir, renamedInodeTimestamps);
                  metaStore->releaseDir(dir->getID());
               }
            }
            else
            {
               auto file = metaStore->referenceFile(&info);
               if (file)
               {
                  fixInodeTimestamp(*file, renamedInodeTimestamps, &info);
                  metaStore->releaseFile(fromParent.getID(), file);
               }
            }
         }
      }

      // handle unlinkInode
      if (unlinkInode && !hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
      {
         unlinkedEntryID = unlinkInode->getEntryID();

         FhgfsOpsErr chunkUnlinkRes = MsgHelperUnlink::unlinkChunkFiles(
            unlinkInode.release(), getMsgHeaderUserID() );

         if (chunkUnlinkRes != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).logErr(std::string("Rename succeeded, but unlinking storage ") +
            "chunk files of the overwritten targetFileName (" + toName + ") failed. " +
            "Entry-ID: " + unlinkedEntryID);

            // we can't do anything about it, so we won't even inform the user
         }
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
   std::string newName, char* serialBuf, size_t serialBufLen, StringVector xattrs,
   std::string& unlinkedEntryID)
{
   LogContext log("RenameV2MsgEx::remoteFileInsert");

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   NumNodeID toNodeID = toDirInfo->getOwnerNodeID();

   LOG_DEBUG_CONTEXT(log, Log_DEBUG,
      "Inserting remote parentID: " + toDirInfo->getEntryID() + "; "
      "newName: '" + newName + "'; "
      "entryID: '" + fromFileInfo->getEntryID() + "'; "
      "node: " + toNodeID.str() );

   // prepare request

   MovingFileInsertMsg insertMsg(fromFileInfo, toDirInfo, newName, serialBuf, serialBufLen);

   RequestResponseArgs rrArgs(NULL, &insertMsg, NETMSGTYPE_MovingFileInsertResp);

   RequestResponseNode rrNode(toNodeID, app->getMetaNodes() );
   rrNode.setTargetStates(app->getMetaStateStore() );
   if (toDirInfo->getIsBuddyMirrored())
      rrNode.setMirrorInfo(app->getMetaBuddyGroupMapper(), false);

   // send request and receive response

   MsgHelperXAttr::StreamXAttrState streamState(*fromFileInfo, std::move(xattrs));
   insertMsg.registerStreamoutHook(rrArgs, streamState);

   FhgfsOpsErr requestRes = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

   if(requestRes != FhgfsOpsErr_SUCCESS)
   { // communication error
      log.log(Log_WARNING,
         "Communication with metadata sever failed. nodeID: " + toNodeID.str() );
      return requestRes;
   }

   // correct response type received

   MovingFileInsertRespMsg* insertRespMsg = (MovingFileInsertRespMsg*)rrArgs.outRespMsg;

   retVal = insertRespMsg->getResult();

   unsigned unlinkedInodeBufLen = insertRespMsg->getInodeBufLen();
   if (unlinkedInodeBufLen)
   {
      const char* unlinkedInodeBuf = insertRespMsg->getInodeBuf();
      FileInode* toUnlinkInode = new FileInode();

      Deserializer des(unlinkedInodeBuf, unlinkedInodeBufLen);
      toUnlinkInode->deserializeMetaData(des);
      if(unlikely(!des.good()))
      { // deserialization of received inode failed (should never happen)
         log.logErr("Failed to deserialize unlinked file inode. nodeID: " + toNodeID.str() );
         delete(toUnlinkInode);
      }
      else
      {
         MsgHelperUnlink::unlinkChunkFiles(
            toUnlinkInode, getMsgHeaderUserID() ); // destructs toUnlinkInode
      }
   }

   if(retVal != FhgfsOpsErr_SUCCESS)
   { // error: remote file not inserted
      LOG_DEBUG_CONTEXT(log, Log_NOTICE,
         "Metadata server was unable to insert file. "
         "nodeID: " + toNodeID.str() + "; "
         "Error: " + FhgfsOpsErrTk::toErrString(retVal) );
   }
   else
   {
      // success: remote file inserted
      LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Metadata server inserted file. "
         "nodeID: " + toNodeID.str() );
   }

   return retVal;
}

/**
 * Insert a remote directory dir-entry
 */
FhgfsOpsErr RenameV2MsgEx::remoteDirInsert(EntryInfo* toDirInfo,  const std::string& newName,
   char* serialBuf, size_t serialBufLen)
{
   LogContext log("RenameV2MsgEx::remoteDirInsert");

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   NumNodeID toNodeID = toDirInfo->getOwnerNodeID();

   LOG_DEBUG_CONTEXT(log, Log_DEBUG,
      "Inserting remote parentID: " + toDirInfo->getEntryID() + "; "
      "newName: '" + newName + "'; "
      "node: " + toNodeID.str() );

   // prepare request

   MovingDirInsertMsg insertMsg(toDirInfo, newName, serialBuf, serialBufLen);

   RequestResponseArgs rrArgs(NULL, &insertMsg, NETMSGTYPE_MovingDirInsertResp);

   RequestResponseNode rrNode(toNodeID, app->getMetaNodes() );
   rrNode.setTargetStates(app->getMetaStateStore() );
   if (toDirInfo->getIsBuddyMirrored())
      rrNode.setMirrorInfo(app->getMetaBuddyGroupMapper(), false);


   // send request and receive response

   FhgfsOpsErr requestRes = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

   if(requestRes != FhgfsOpsErr_SUCCESS)
   { // communication error
      log.log(Log_WARNING, "Communication with metadata server failed. nodeID: " + toNodeID.str() );
      return requestRes;
   }

   // correct response type received
   MovingDirInsertRespMsg* insertRespMsg = (MovingDirInsertRespMsg*)rrArgs.outRespMsg;

   retVal = insertRespMsg->getResult();
   if(retVal != FhgfsOpsErr_SUCCESS)
   { // error: remote dir not inserted
      LOG_DEBUG_CONTEXT(log, Log_NOTICE,
         "Metdata server was unable to insert directory. "
         "nodeID: " + toNodeID.str() + "; "
         "Error: " + FhgfsOpsErrTk::toErrString(retVal) );
   }
   else
   {
      // success: remote dir inserted
      LOG_DEBUG_CONTEXT(log, Log_DEBUG,
         "Metadata server inserted directory. "
         "nodeID: " + toNodeID.str() );
   }

   return retVal;
}

/**
 * Set the new parent information to the inode of renamedDirEntryInfo
 */
FhgfsOpsErr RenameV2MsgEx::updateRenamedDirInode(EntryInfo* renamedDirEntryInfo,
   EntryInfo* toDirInfo)
{
   LogContext log("RenameV2MsgEx::updateRenamedDirInode");

   const std::string& parentEntryID = toDirInfo->getEntryID();
   renamedDirEntryInfo->setParentEntryID(parentEntryID);

   App* app = Program::getApp();
   NumNodeID toNodeID = renamedDirEntryInfo->getOwnerNodeID();

   LOG_DEBUG_CONTEXT(log, Log_DEBUG,
      "Update remote inode: " + renamedDirEntryInfo->getEntryID() + "; "
      "node: " + toNodeID.str() );

   // prepare request

   UpdateDirParentMsg updateMsg(renamedDirEntryInfo, toDirInfo->getOwnerNodeID() );

   RequestResponseArgs rrArgs(NULL, &updateMsg, NETMSGTYPE_UpdateDirParentResp);

   RequestResponseNode rrNode(toNodeID, app->getMetaNodes() );
   rrNode.setTargetStates(app->getMetaStateStore() );
   if (renamedDirEntryInfo->getIsBuddyMirrored())
      rrNode.setMirrorInfo(app->getMetaBuddyGroupMapper(), false);

   // send request and receive response

   FhgfsOpsErr requestRes = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

   if(requestRes != FhgfsOpsErr_SUCCESS)
   { // communication error
      log.log(Log_WARNING,
         "Communication with metadata server failed. nodeID: " + toNodeID.str() );
      return requestRes;
   }

   // correct response type received
   UpdateDirParentRespMsg* updateRespMsg = (UpdateDirParentRespMsg*)rrArgs.outRespMsg;

   FhgfsOpsErr retVal = (FhgfsOpsErr)updateRespMsg->getValue();
   if(retVal != FhgfsOpsErr_SUCCESS)
   { // error
      LOG_DEBUG_CONTEXT(log, Log_NOTICE,
         "Failed to update ParentEntryID: " + renamedDirEntryInfo->getEntryID() + "; "
         "nodeID: " + toNodeID.str() + "; "
         "Error: " + FhgfsOpsErrTk::toErrString(retVal) );
   }

   return retVal;
}
