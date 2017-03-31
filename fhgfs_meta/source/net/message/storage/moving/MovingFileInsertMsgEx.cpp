#include <program/Program.h>
#include <common/net/message/storage/moving/MovingFileInsertMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperChunkBacklinks.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <net/msghelpers/MsgHelperXAttr.h>

#include "MovingFileInsertMsgEx.h"


bool MovingFileInsertMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("MovingFileInsertMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a MovingFileInsertMsg from: " + ctx.peerName() );

   return BaseType::processIncoming(ctx);
}


std::unique_ptr<MirroredMessageResponseState> MovingFileInsertMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   EntryInfo* fromFileInfo = this->getFromFileInfo();
   EntryInfo* toDirInfo    = this->getToDirInfo();
   std::string newName     = this->getNewName();

   std::unique_ptr<FileInode> unlinkInode;
   unsigned inodeBufLen;
   std::unique_ptr<char[]> inodeBuf;

   DirInode* toDir = metaStore->referenceDir(toDirInfo->getEntryID(),
         toDirInfo->getIsBuddyMirrored(), true);
   if (!toDir)
      return boost::make_unique<MovingFileInsertResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   FhgfsOpsErr moveRes = metaStore->moveRemoteFileInsert(
      fromFileInfo, *toDir, newName,
      getSerialBuf(), getSerialBufLen(), &unlinkInode, newFileInfo, std::get<0>(lockState),
      std::get<1>(lockState));
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

   // update backlinks
   if (Program::getApp()->getConfig()->getStoreBacklinksEnabled())
   {
      EntryInfo* toDirInfo = this->getToDirInfo();
      std::string newName  = this->getNewName();
      MsgHelperChunkBacklinks::updateBacklink(toDirInfo->getEntryID(),
         toDirInfo->getIsBuddyMirrored(), newName);
   }

   if(unlinkInode)
   {
      inodeBuf.reset(new (std::nothrow) char[META_SERBUF_SIZE]);
      if (unlikely(!inodeBuf) )
      {  // out of memory, we are going to leak an inode and chunks
         inodeBufLen = 0;
         LOG(ERR, "Malloc failed, leaking chunks", as("inodeID", unlinkInode->getEntryID()));
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
         std::move(inodeBuf));

xattr_error:
   // TODO: reset toDir timestamps back to original values (before the file was created)
   MsgHelperUnlink::unlinkMetaFile(*toDir, newName, NULL);
   metaStore->releaseDir(toDir->getID());

   return boost::make_unique<MovingFileInsertResponseState>(retVal);
}

bool MovingFileInsertMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   return sendToSecondary(ctx, *this, NETMSGTYPE_MovingFileInsertResp) == FhgfsOpsErr_SUCCESS;
}
