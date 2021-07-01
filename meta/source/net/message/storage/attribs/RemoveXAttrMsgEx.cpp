#include <program/Program.h>
#include <common/net/message/storage/attribs/RemoveXAttrRespMsg.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <session/EntryLock.h>
#include "RemoveXAttrMsgEx.h"


bool RemoveXAttrMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "RemoveXAttrMsg incoming";
   EntryInfo* entryInfo = this->getEntryInfo();
   const std::string& name = this->getName();

   LOG_DEBUG(logContext, Log_DEBUG, "name: " + name + ";");

   LOG_DEBUG("RemoveXAttrMsgEx::processIncoming", Log_DEBUG,
      "ParentID: " + entryInfo->getParentEntryID() + " EntryID: " +
      entryInfo->getEntryID() + " BuddyMirrored: " +
      (entryInfo->getIsBuddyMirrored() ? "Yes" : "No") + " Secondary: " +
      (hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) ? "Yes" : "No"));
#endif

   BaseType::processIncoming(ctx);

   updateNodeOp(ctx, MetaOpCounter_REMOVEXATTR);

   return true;
}

std::tuple<FileIDLock, FileIDLock> RemoveXAttrMsgEx::lock(EntryLockStore& store)
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

std::unique_ptr<MirroredMessageResponseState> RemoveXAttrMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   if (!Program::getApp()->getConfig()->getStoreClientXAttrs() ) // xattrs disabled in config
   {
      LOG(GENERAL, ERR, "Received a RemoveXAttrMsg, "
                        "but client-side extended attributes are disabled in config.");
      return boost::make_unique<ResponseState>(FhgfsOpsErr_NOTSUPP);
   }

   auto rmRes = MsgHelperXAttr::removexattr(getEntryInfo(), getName());

   if (rmRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
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

   return boost::make_unique<ResponseState>(rmRes);
}

void RemoveXAttrMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_RemoveXAttrResp);
}
