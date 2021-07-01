#include <program/Program.h>
#include <common/net/message/storage/attribs/SetXAttrRespMsg.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <session/EntryLock.h>
#include "SetXAttrMsgEx.h"

std::tuple<FileIDLock, FileIDLock> SetXAttrMsgEx::lock(EntryLockStore& store)
{
   if (getEntryInfo()->getEntryType() == DirEntryType_DIRECTORY)
      return std::make_tuple(
            FileIDLock(),
            FileIDLock(&store, getEntryInfo()->getEntryID(), true));
   else
      return std::make_tuple(
            FileIDLock(&store, getEntryInfo()->getEntryID(), true),
            FileIDLock());
}

bool SetXAttrMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "SetXAttrMsg incoming";
   EntryInfo* entryInfo = this->getEntryInfo();
   const std::string& name = this->getName();

   LOG_DEBUG(logContext, Log_DEBUG, "name: " + name + ";");

   LOG_DEBUG("SetXAttrMsgEx::processIncoming", Log_DEBUG,
      "ParentID: " + entryInfo->getParentEntryID() + " EntryID: " +
      entryInfo->getEntryID() + " BuddyMirrored: " +
      (entryInfo->getIsBuddyMirrored() ? "Yes" : "No") + " Secondary: " +
      (hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) ? "Yes" : "No"));
#endif

   BaseType::processIncoming(ctx);

   updateNodeOp(ctx, MetaOpCounter_SETXATTR);

   return true;
}

std::unique_ptr<MirroredMessageResponseState> SetXAttrMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   if (!Program::getApp()->getConfig()->getStoreClientXAttrs())
   {
      LOG(GENERAL, ERR,
            "Received a SetXAttrMsg, but client-side extended attributes are disabled in config.");
      return boost::make_unique<ResponseState>(FhgfsOpsErr_NOTSUPP);
   }

   auto setXAttrRes = MsgHelperXAttr::setxattr(getEntryInfo(), getName(), getValue(), getFlags());

   if (setXAttrRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
   {
      MetaStore* metaStore = Program::getApp()->getMetaStore();

      if (DirEntryType_ISDIR(getEntryInfo()->getEntryType()))
      {
         auto dir = metaStore->referenceDir(getEntryInfo()->getEntryID(),
               getEntryInfo()->getIsBuddyMirrored(), true);
         if (dir)
         {
            fixInodeTimestamp(*dir, inodeTimestamps);
            metaStore->releaseDir(dir->getID());
         }
      }
      else
      {
         auto file = metaStore->referenceFile(getEntryInfo());
         if (file)
         {
            fixInodeTimestamp(*file, inodeTimestamps, getEntryInfo());
            metaStore->releaseFile(getEntryInfo()->getParentEntryID(), file);
         }
      }
   }

   return boost::make_unique<ResponseState>(setXAttrRes);
}

void SetXAttrMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_SetXAttrResp);
}
