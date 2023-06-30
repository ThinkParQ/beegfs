#include <program/Program.h>
#include <common/net/message/storage/moving/MovingFileInsertMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <net/msghelpers/MsgHelperXAttr.h>

#include "MovingFileInsertMsgEx.h"

std::tuple<FileIDLock, FileIDLock, FileIDLock, ParentNameLock> MovingFileInsertMsgEx::lock(
      EntryLockStore& store)
{
   // we must not lock the directory if it is owned by the current node. if it is, the
   // current message was also sent by the local node, specifically by a rename msg, which
   // also locks the directory for write
   if (rctx->isLocallyGenerated())
      return {};

   FileIDLock dirLock(&store, getToDirInfo()->getEntryID(), true);

   ParentNameLock nameLock(&store, getToDirInfo()->getEntryID(), getNewName());

   FileIDLock newLock;
   FileIDLock unlinkedLock;

   auto dir = Program::getApp()->getMetaStore()->referenceDir(getToDirInfo()->getEntryID(),
         getToDirInfo()->getIsBuddyMirrored(), true);
   if (dir)
   {
      FileInode newInode;
      Deserializer des(getSerialBuf(), getSerialBufLen());
      newInode.deserializeMetaData(des);
      if (des.good())
      {
         std::string unlinkedID = newInode.getEntryID();

         EntryInfo unlinkedInfo;
         dir->getFileEntryInfo(getNewName(), unlinkedInfo);
         if (DirEntryType_ISFILE(unlinkedInfo.getEntryType()))
            unlinkedID = unlinkedInfo.getEntryID();

         if (newInode.getEntryID() < unlinkedID)
         {
            newLock = {&store, newInode.getEntryID(), true};
            unlinkedLock = {&store, unlinkedID, true};
         }
         else if (newInode.getEntryID() == unlinkedID)
         {
            newLock = {&store, newInode.getEntryID(), true};
         }
         else
         {
            unlinkedLock = {&store, unlinkedID, true};
            newLock = {&store, newInode.getEntryID(), true};
         }
      }

      Program::getApp()->getMetaStore()->releaseDir(dir->getID());
   }

   return std::make_tuple(
         std::move(newLock),
         std::move(unlinkedLock),
         std::move(dirLock),
         std::move(nameLock));
}

bool MovingFileInsertMsgEx::processIncoming(ResponseContext& ctx)
{
   rctx = &ctx;

   return BaseType::processIncoming(ctx);
}


std::unique_ptr<MirroredMessageResponseState> MovingFileInsertMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   EntryInfo* fromFileInfo = this->getFromFileInfo();
   EntryInfo* toDirInfo    = this->getToDirInfo();
   std::string newName     = this->getNewName();

   EntryInfo overWrittenEntryInfo;
   std::unique_ptr<FileInode> unlinkInode;
   unsigned inodeBufLen;
   std::unique_ptr<char[]> inodeBuf;

   DirInode* toDir = metaStore->referenceDir(toDirInfo->getEntryID(),
         toDirInfo->getIsBuddyMirrored(), true);
   if (!toDir)
      return boost::make_unique<MovingFileInsertResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   FhgfsOpsErr moveRes = metaStore->moveRemoteFileInsert(
      fromFileInfo, *toDir, newName, getSerialBuf(), getSerialBufLen(), &unlinkInode,
      &overWrittenEntryInfo, newFileInfo);
   if (moveRes != FhgfsOpsErr_SUCCESS)
   {
      metaStore->releaseDir(toDir->getID());
      return boost::make_unique<MovingFileInsertResponseState>(moveRes);
   }

   std::string xattrName;
   CharVector xattrValue;

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   while (isMsgHeaderFeatureFlagSet(MOVINGFILEINSERTMSG_FLAG_HAS_XATTRS))
   {
      retVal = MsgHelperXAttr::StreamXAttrState::readNextXAttr(ctx.getSocket(), xattrName,
            xattrValue);
      if (retVal == FhgfsOpsErr_SUCCESS)
         break;
      else if (retVal != FhgfsOpsErr_AGAIN)
         goto xattr_error;

      retVal = MsgHelperXAttr::setxattr(&newFileInfo, xattrName, xattrValue, 0);
      if (retVal != FhgfsOpsErr_SUCCESS)
         goto xattr_error;

      xattrNames.push_back(xattrName);
   }

   if(unlinkInode)
   {
      inodeBuf.reset(new (std::nothrow) char[META_SERBUF_SIZE]);
      if (unlikely(!inodeBuf) )
      {  // out of memory, we are going to leak an inode and chunks
         inodeBufLen = 0;
         LOG(GENERAL, ERR, "Malloc failed, leaking chunks", ("inodeID", unlinkInode->getEntryID()));
      }
      else
      {
         Serializer ser(inodeBuf.get(), META_SERBUF_SIZE);
         unlinkInode->serializeMetaData(ser);
         inodeBufLen = ser.size();
      }
   }
   else
   {  // no file overwritten
      inodeBufLen = 0;
   }

   if (shouldFixTimestamps())
   {
      fixInodeTimestamp(*toDir, dirTimestamps);

      if (auto newFile = metaStore->referenceFile(&newFileInfo))
      {
         fixInodeTimestamp(*newFile, fileTimestamps, &newFileInfo);
         metaStore->releaseFile(toDir->getID(), newFile);
      }
   }

   metaStore->releaseDir(toDir->getID());

   return boost::make_unique<MovingFileInsertResponseState>(FhgfsOpsErr_SUCCESS, inodeBufLen,
      std::move(inodeBuf), overWrittenEntryInfo);

xattr_error:
   MsgHelperUnlink::unlinkMetaFile(*toDir, newName, NULL);
   metaStore->releaseDir(toDir->getID());

   return boost::make_unique<MovingFileInsertResponseState>(retVal);
}

void MovingFileInsertMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_MovingFileInsertResp);
}
