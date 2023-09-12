#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/UpdateDirParentMsg.h>
#include <common/net/message/storage/attribs/UpdateDirParentRespMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileInodeMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileInodeRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/MetadataTk.h>
#include <components/FileEventLogger.h>
#include <components/ModificationEventFlusher.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <program/Program.h>
#include <session/EntryLock.h>
#include "RenameV2MsgEx.h"

#include <boost/scoped_array.hpp>

namespace {
struct DirHandle {
   MetaStore* metaStore;
   const EntryInfo* ei;

   DirHandle(MetaStore* metaStore, const EntryInfo* ei): metaStore(metaStore), ei(ei) {}

   DirHandle(const DirHandle&) = delete;
   DirHandle(DirHandle&&) = delete;

   DirHandle& operator=(const DirHandle&) = delete;
   DirHandle& operator=(DirHandle&&) = delete;

   ~DirHandle() {
      metaStore->releaseDir(ei->getEntryID());
   }
};
}

RenameV2Locks RenameV2MsgEx::lock(EntryLockStore& store)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   // if the directory could not be referenced it does not exist on the current node. this will
   // cause the operation to fail lateron during executeLocally() when we reference the same
   // directory again. since we cannot do anything without having access to the source directory,
   // and since no directory with the same id as the source directory can appear after the source
   // directory has been removed, we can safely unlock everything right here and continue without
   // blocking other workers on the (probably live) target directory.
   DirInode* fromDir = metaStore->referenceDir(getFromDirInfo()->getEntryID(),
         getFromDirInfo()->getIsBuddyMirrored(), true);
   if (!fromDir)
      return {};

   const DirHandle _from(metaStore, getFromDirInfo());

   DirInode* toDir = metaStore->referenceDir(getToDirInfo()->getEntryID(),
         getToDirInfo()->getIsBuddyMirrored(), true);

   if (!toDir)
      return {};

   const DirHandle _to(metaStore, getToDirInfo());

   for (;;) {
      RenameV2Locks result;

      EntryInfo fromFileInfo;
      EntryInfo toFileInfo;

      fromDir->getFileEntryInfo(getOldName(), fromFileInfo);
      bool toFileExists = toDir->getFileEntryInfo(getNewName(), toFileInfo);

      if (toFileExists && DirEntryType_ISFILE(toFileInfo.getEntryType()))
      {
         // lock hash dir of tofile inode only if:
         // 1) its a file and
         // 2) its a non-inlined inode and
         // 3) resynch job is running
         if (resyncJob && resyncJob->isRunning() && !toFileInfo.getIsInlined())
            result.toFileHashLock = {&store, MetaStorageTk::getMetaInodeHash(toFileInfo.getEntryID())};
      }

      {
         std::map<std::string, FileIDLock*> lockOrder;

         lockOrder.insert(std::make_pair(getFromDirInfo()->getEntryID(), &result.fromDirLock));
         lockOrder.insert(std::make_pair(getToDirInfo()->getEntryID(), &result.toDirLock));
         if (DirEntryType_ISDIR(fromFileInfo.getEntryType()))
            lockOrder.insert(std::make_pair(fromFileInfo.getEntryID(), &result.fromFileLockD));

         for (auto it = lockOrder.begin(); it != lockOrder.end(); ++it)
            *it->second = {&store, it->first, true};
      }

      // we might have locked fromFileLockD before fromDirLock due to ordering. resolve the source
      // once more and check that we still refer to the same id, otherwise retry until we have the
      // correct inode.
      // if the name went away we don't have to retry (it can't be created while the dir is locked),
      // but retrying is simpler to do.
      EntryInfo fromFileInfoCheck;
      fromDir->getFileEntryInfo(getOldName(), fromFileInfoCheck);
      if (fromFileInfo.getEntryID() != fromFileInfoCheck.getEntryID())
         continue;

      // take care about lock ordering! see MirroredMessage::lock()
      // since directories are locked for read, and by the same id as the (parent,name) tuples,the
      // same ordering applies.
      if (getFromDirInfo()->getEntryID() < getToDirInfo()->getEntryID())
      {
         result.fromNameLock = {&store, getFromDirInfo()->getEntryID(), getOldName()};
         result.toNameLock = {&store, getToDirInfo()->getEntryID(), getNewName()};
      }
      else if (getFromDirInfo()->getEntryID() == getToDirInfo()->getEntryID())
      {
         if (getOldName() < getNewName())
         {
            result.fromNameLock = {&store, getFromDirInfo()->getEntryID(), getOldName()};
            result.toNameLock = {&store, getToDirInfo()->getEntryID(), getNewName()};
         }
         else if (getOldName() == getNewName())
         {
            result.fromNameLock = {&store, getFromDirInfo()->getEntryID(), getOldName()};
         }
         else
         {
            result.toNameLock = {&store, getToDirInfo()->getEntryID(), getNewName()};
            result.fromNameLock = {&store, getFromDirInfo()->getEntryID(), getOldName()};
         }
      }
      else
      {
         result.toNameLock = {&store, getToDirInfo()->getEntryID(), getNewName()};
         result.fromNameLock = {&store, getFromDirInfo()->getEntryID(), getOldName()};
      }

      if (DirEntryType_ISFILE(fromFileInfo.getEntryType()) && fromFileInfo.getIsInlined())
      {
         if (DirEntryType_ISFILE(toFileInfo.getEntryType()) && toFileInfo.getIsInlined())
         {
            if (fromFileInfo.getEntryID() < toFileInfo.getEntryID())
            {
               result.fromFileLockF = {&store, fromFileInfo.getEntryID(), true};
               result.unlinkedFileLock = {&store, toFileInfo.getEntryID(), true};
            }
            else if (fromFileInfo.getEntryID() == toFileInfo.getEntryID())
            {
               result.fromFileLockF = {&store, fromFileInfo.getEntryID(), true};
            }
            else
            {
               result.unlinkedFileLock = {&store, toFileInfo.getEntryID(), true};
               result.fromFileLockF = {&store, fromFileInfo.getEntryID(), true};
            }
         }
         else
         {
            result.fromFileLockF = {&store, fromFileInfo.getEntryID(), true};
         }
      }

      return result;
   }
}

bool RenameV2MsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DEBUG(__func__, Log_DEBUG, "FromDirID: " + getFromDirInfo()->getEntryID() + "; "
      "oldName: '" + getOldName() + "'; "
      "ToDirID: " + getToDirInfo()->getEntryID() + "; "
      "newName: '" + getNewName() + "'");

   BaseType::processIncoming(ctx);

   // update operation counters
   updateNodeOp(ctx, MetaOpCounter_RENAME);

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

   const auto fileEventLogEnabled = !isSecondary
                                    && getFileEvent()
                                    && Program::getApp()->getFileEventLogger();

   if (modEventLoggingEnabled || fileEventLogEnabled)
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

   if (!isSecondary && Program::getApp()->getFileEventLogger() && getFileEvent())
   {
         Program::getApp()->getFileEventLogger()->log(
                  *getFileEvent(),
                  movedEntryID,
                  getFromDirInfo()->getEntryID(),
                  getToDirInfo()->getEntryID());
   }

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

   return boost::make_unique<ResponseState>(renameRes);
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

void RenameV2MsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_RenameResp);
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
   if (!dirEntryCopyRes)
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

         if (!hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            updateRenamedDirInode(&removedInfo, toDirInfo);
      }
      else
      {
         LogContext(logContext).log(Log_CRITICAL,
            std::string("Failed to remove fromDir: ") + oldName +
            " Error: " + boost::lexical_cast<std::string>(retVal));
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
   if(!getRes )
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

      LOG_DEBUG(logContext, Log_SPAM, "Method: remote file move."); // debug in

      if (app->getConfig()->getStoreClientXAttrs() && fromFileEntryInfo.getIsInlined())
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
   if (retVal == FhgfsOpsErr_SUCCESS)
   {
      FhgfsOpsErr unlinkRes;

      if (fromFileEntryInfo.getIsInlined())
      {
         // We are not interested in the inode here , as we are not going to delete storage chunks.
         EntryInfo entryInfo;
         unlinkRes = metaStore->unlinkFile(fromParent, oldName, &entryInfo, NULL);
      }
      else
      {
         // only unlink dentry-by-filename for nonInlined inode(s)
         DirEntry oldDentry(oldName);
         if (fromParent.getDentry(oldName, oldDentry))
            unlinkRes = fromParent.unlinkDirEntry(oldName, &oldDentry, DirEntry_UNLINK_FILENAME);
         else
            unlinkRes = FhgfsOpsErr_PATHNOTEXISTS;
      }

      if (unlikely (unlinkRes) )
      {
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
 */
FhgfsOpsErr RenameV2MsgEx::renameInSameDir(DirInode& fromParent, const std::string& oldName,
   const std::string& toName, std::string& unlinkedEntryID)
{
   const char* logContext = "RenameV2MsgEx::renameInSameDir";
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   /* we are passing here the very same fromParent pointer also a toParent pointer, which is
    * essential in order not to dead-lock */

   std::unique_ptr<FileInode> unlinkInode; // inode belong to a possibly existing toName file
   DirEntry* overWrittenEntry = NULL;
   bool wasInlined; // to determine if overwritten inode was inlined or not

   FhgfsOpsErr renameRes = metaStore->renameInSameDir(fromParent, oldName, toName,
         &unlinkInode, overWrittenEntry, wasInlined);

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

      if (overWrittenEntry && !overWrittenEntry->getIsInodeInlined())
      {
         EntryInfo overWrittenEntryInfo;
         overWrittenEntry->getEntryInfo(fromParent.getID(), 0, &overWrittenEntryInfo);

         if (wasInlined)
         {
            // if overwritten file previously had an inlined inode which got de-inlined because
            // it was IN_USE during rename operation - in this case, its inode should already be
            // linked to disposal directory and will be removed upon close()
            return renameRes;
         }
         else if (!hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
         {
            // if overwritten file had a non-inlined inode - update hardlink count and remove
            // inode and chunk files if link count becomes zero
            unlinkRemoteFileInode(&overWrittenEntryInfo);
         }
      }

      // handle unlinkInode for inlined inode(s)
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
   const auto insertRespMsg = (MovingFileInsertRespMsg*)rrArgs.outRespMsg.get();
   retVal = insertRespMsg->getResult();

   // handle unlink of chunk files for inlined inode(s)
   // handle unlink of inode for non-inlined inode(s)
   EntryInfo* overWrittenInfo = insertRespMsg->getOverWrittenEntryInfo();
   if (overWrittenInfo->getEntryType() != DirEntryType_INVALID)
   {
      if (overWrittenInfo->getIsInlined())
      {
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
            else if (toUnlinkInode->getIsInlined())
            {
               MsgHelperUnlink::unlinkChunkFiles(
                  toUnlinkInode, getMsgHeaderUserID() ); // destructs toUnlinkInode
            }
         }
      }
      else
      {
         unlinkRemoteFileInode(overWrittenInfo);
      }
   }

   if (retVal != FhgfsOpsErr_SUCCESS)
   {
      // error: remote file not inserted
      LOG_DEBUG_CONTEXT(log, Log_NOTICE,
         "Metadata server was unable to insert file. "
         "nodeID: " + toNodeID.str() + "; "
         "Error: " + boost::lexical_cast<std::string>(retVal));
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
   const auto insertRespMsg = (const MovingDirInsertRespMsg*)rrArgs.outRespMsg.get();

   retVal = insertRespMsg->getResult();
   if(retVal != FhgfsOpsErr_SUCCESS)
   { // error: remote dir not inserted
      LOG_DEBUG_CONTEXT(log, Log_NOTICE,
         "Metdata server was unable to insert directory. "
         "nodeID: " + toNodeID.str() + "; "
         "Error: " + boost::lexical_cast<std::string>(retVal));
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
   const auto updateRespMsg = (const UpdateDirParentRespMsg*)rrArgs.outRespMsg.get();

   FhgfsOpsErr retVal = (FhgfsOpsErr)updateRespMsg->getValue();
   if(retVal != FhgfsOpsErr_SUCCESS)
   { // error
      LOG_DEBUG_CONTEXT(log, Log_NOTICE,
         "Failed to update ParentEntryID: " + renamedDirEntryInfo->getEntryID() + "; "
         "nodeID: " + toNodeID.str() + "; "
         "Error: " + boost::lexical_cast<std::string>(retVal));
   }

   return retVal;
}

/*
 * Handle unlink for a non-inlined inode
 * Decrement hardLink count, if linkCount becomes zero then remove inode and chunk files
 */
FhgfsOpsErr RenameV2MsgEx::unlinkRemoteFileInode(EntryInfo* entryInfo)
{
   const char* logContext = "Unlink remote file inode";
   App* app = Program::getApp();
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   NumNodeID ownerNodeID = entryInfo->getOwnerNodeID();

   UnlinkLocalFileInodeMsg unlinkInodeMsg(entryInfo);
   RequestResponseArgs rrArgs(NULL, &unlinkInodeMsg, NETMSGTYPE_UnlinkLocalFileInodeResp);
   RequestResponseNode rrNode(ownerNodeID, app->getMetaNodes());

   if (entryInfo->getIsBuddyMirrored())
      rrNode.setMirrorInfo(Program::getApp()->getMetaBuddyGroupMapper(), false);

   do
   {
      retVal = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

      if (unlikely(retVal != FhgfsOpsErr_SUCCESS))
      {
         LogContext(logContext).log(Log_WARNING,
            "Communication with metadata server failed. "
            "nodeID: " + ownerNodeID.str() + "; " +
            "entryID: " + entryInfo->getEntryID().c_str());
         break;
      }

      // response received
      const auto unlinkFileInodeRespMsg = (UnlinkLocalFileInodeRespMsg*) rrArgs.outRespMsg.get();
      retVal = unlinkFileInodeRespMsg->getResult();
      if (retVal != FhgfsOpsErr_SUCCESS)
      {
         // error: either inode file doesn't exists or some other error happened
         LogContext(logContext).logErr("unlink of inode failed! "
            "nodeID: " + ownerNodeID.str() + "; " +
            "entryID: " + entryInfo->getEntryID().c_str());
         break;
      }
   } while (false);

   return retVal;
}
