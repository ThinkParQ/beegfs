#include <program/Program.h>
#include <common/net/message/storage/moving/MovingDirInsertMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include "MovingDirInsertMsgEx.h"


bool MovingDirInsertMsgEx::processIncoming(ResponseContext& ctx)
{
   rctx = &ctx;

   return BaseType::processIncoming(ctx);
}


std::unique_ptr<MirroredMessageResponseState> MovingDirInsertMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   EntryInfo* toDirInfo = this->getToDirInfo();

   // reference parent
   DirInode* parentDir = metaStore->referenceDir(toDirInfo->getEntryID(),
      toDirInfo->getIsBuddyMirrored(), true);
   if(!parentDir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   /* create dir-entry and add information about its inode from the given buffer */

   const std::string& newName = this->getNewName();
   const char* buf = this->getSerialBuf();
   Deserializer des(buf, getSerialBufLen());
   DirEntry newDirEntry(newName);

   newDirEntry.deserializeDentry(des);
   if (!des.good())
   {
      LogContext("File rename").logErr("Bug: Deserialization of remote buffer failed. Are all "
         "meta servers running the same version?" );

      retVal = FhgfsOpsErr_INTERNAL;
      goto out;
   }

   if (newDirEntry.getEntryID() == parentDir->getID() )
   { // attempt to rename a dir into itself
      retVal = FhgfsOpsErr_INVAL;
      goto out;
   }

   retVal = parentDir->makeDirEntry(newDirEntry);

   if (retVal == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(*parentDir, dirTimestamps);

   if (retVal != FhgfsOpsErr_SUCCESS && retVal != FhgfsOpsErr_EXISTS)
      retVal = FhgfsOpsErr_INTERNAL;

out:
   metaStore->releaseDir(toDirInfo->getEntryID());
   return boost::make_unique<ResponseState>(retVal);
}

void MovingDirInsertMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_MovingDirInsertResp);
}
