#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/storage/StatData.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <program/Program.h>
#include "MkFileMsgEx.h"

std::tuple<FileIDLock, ParentNameLock, FileIDLock> MkFileMsgEx::lock(EntryLockStore& store)
{
   FileIDLock dirLock(&store, getParentInfo()->getEntryID(), true);
   ParentNameLock dentryLock(&store, getParentInfo()->getEntryID(), getNewName());
   FileIDLock fileLock(&store, newEntryID, true);

   return std::make_tuple(
         std::move(dirLock),
         std::move(dentryLock),
         std::move(fileLock));
}


bool MkFileMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "MkFileMsg incoming";
#endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_DEBUG, "parentEntryID: " +
      getParentInfo()->getEntryID() + " newFileName: " + getNewName() );

   LOG_DEBUG(logContext, Log_DEBUG,
      "BuddyMirrored: " + std::string(getParentInfo()->getIsBuddyMirrored() ? "'Yes'" : "'No'") +
      " Secondary: " + std::string(hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) ?
         "Yes" : "No") );

   if (!hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
      createTime = TimeAbs().getTimeval()->tv_sec;

   mkDetails = {getNewName(), getUserID(), getGroupID(), getMode(), getUmask(), createTime};

   if (getParentInfo()->getIsBuddyMirrored())
   {
      if (!hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
         newEntryID = StorageTk::generateFileID(Program::getApp()->getLocalNode().getNumID());
      else
         newEntryID = getNewEntryID();

      mkDetails.setNewEntryID(newEntryID.c_str());
   }

   BaseType::processIncoming(ctx);

   updateNodeOp(ctx, MetaOpCounter_MKFILE);

   return true;
}

std::unique_ptr<MirroredMessageResponseState> MkFileMsgEx::executeLocally(ResponseContext& ctx,
      bool isSecondary)
{
   if (isSecondary)
      return executeSecondary();
   else
      return executePrimary();
}

std::unique_ptr<MkFileMsgEx::ResponseState> MkFileMsgEx::executePrimary()
{
   DirInode* dir = Program::getApp()->getMetaStore()->referenceDir(
         getParentInfo()->getEntryID(), getParentInfo()->getIsBuddyMirrored(), true);
   if (!dir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS, entryInfo);

   FhgfsOpsErr mkRes;
   if (isMsgHeaderFeatureFlagSet(MKFILEMSG_FLAG_STORAGEPOOLID))
   {
      mkRes = MsgHelperMkFile::mkFile(*dir, &mkDetails, &getPreferredNodes(),
            getNumTargets(), getChunkSize(), NULL, &entryInfo, &inodeData, storagePoolId);
   }
   else
   {
      mkRes = MsgHelperMkFile::mkFile(*dir, &mkDetails, &getPreferredNodes(),
            getNumTargets(), getChunkSize(), NULL, &entryInfo, &inodeData);
   }

   if (mkRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(*dir, dirTimestamps);

   Program::getApp()->getMetaStore()->releaseDir(dir->getID());

   if (mkRes == FhgfsOpsErr_SUCCESS && Program::getApp()->getFileEventLogger() && getFileEvent())
   {
         Program::getApp()->getFileEventLogger()->log(
                  *getFileEvent(),
                  entryInfo.getEntryID(),
                  entryInfo.getParentEntryID());
   }

   return boost::make_unique<ResponseState>(mkRes, entryInfo);
}

std::unique_ptr<MkFileMsgEx::ResponseState> MkFileMsgEx::executeSecondary()
{
   StripePattern* stripePattern = getPattern().clone();

   FileInodeStoreData inodeData;

   DirInode* dir = Program::getApp()->getMetaStore()->referenceDir(
         getParentInfo()->getEntryID(), getParentInfo()->getIsBuddyMirrored(), true);
   if (!dir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS, entryInfo);

   FhgfsOpsErr mkRes = MsgHelperMkFile::mkFile(*dir, &mkDetails, &getPreferredNodes(),
      getNumTargets(), getChunkSize(), stripePattern, &entryInfo, &inodeData);

   if (mkRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(*dir, dirTimestamps);

   Program::getApp()->getMetaStore()->releaseDir(dir->getID());

   return boost::make_unique<ResponseState>(mkRes, entryInfo);
}

void MkFileMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   // secondary needs to know the created entryID and stripe pattern, because it needs to use the
   // same information
   setNewEntryID(newEntryID.c_str());
   setPattern(inodeData.getStripePattern());

   sendToSecondary(ctx, *this, NETMSGTYPE_MkFileResp);
}
