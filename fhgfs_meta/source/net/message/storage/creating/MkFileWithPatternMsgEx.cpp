#include <common/net/message/storage/creating/MkFileMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <program/Program.h>
#include <common/net/message/storage/creating/MkLocalFileMsg.h>
#include <common/net/message/storage/creating/MkLocalFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <session/EntryLock.h>
#include "MkFileWithPatternMsgEx.h"

// Called from fhgfs-ctl (online_cfg) to create a file on a specific node.

std::tuple<DirIDLock, ParentNameLock, FileIDLock> MkFileWithPatternMsgEx::lock(
      EntryLockStore& store)
{
   // no need to lock the created file as well, since
   //  a) no other operation can create the same file id
   //  b) until we finish, no part of the system except us knows the new file id
   //  c) if bulk resync gets the file while it is incomplete, individual resync will get it again
   DirIDLock dirLock(&store, getParentInfo()->getEntryID(), true);
   ParentNameLock dentryLock(&store, getParentInfo()->getEntryID(), getNewFileName());
   FileIDLock fileLock(&store, entryID);

   return std::make_tuple(std::move(dirLock), std::move(dentryLock), std::move(fileLock));
}

bool MkFileWithPatternMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "MkFileWithPatternMsg incoming";

      LOG_DEBUG(logContext, 4, "Received a MkFileWithPatternMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

   const EntryInfo* parentInfo = getParentInfo();
   std::string newFileName = getNewFileName();
   EntryInfo newEntryInfo;

   LOG_DEBUG("MkFileWithPatternMsg", Log_DEBUG,
      " parentDirID: " + parentInfo->getEntryID() + " newFileName: " + newFileName
      + " isBuddyMirrored: " + (parentInfo->getIsBuddyMirrored() ? "'true'" : "'false'"));

   // create ID first
   if (parentInfo->getIsBuddyMirrored())
      entryID = StorageTk::generateFileID(Program::getApp()->getLocalNode().getNumID());

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> MkFileWithPatternMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   EntryInfo newEntryInfo;

   MkFileDetails mkDetails(getNewFileName(), getUserID(), getGroupID(), getMode(), getUmask(),
         TimeAbs().getTimeval()->tv_sec);
   if (!entryID.empty())
      mkDetails.setNewEntryID(entryID.c_str());

   FhgfsOpsErr mkRes = mkFile(getParentInfo(), mkDetails, &newEntryInfo, inodeDiskData);

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_MKFILE,
      getMsgHeaderUserID());

   return boost::make_unique<ResponseState>(mkRes, std::move(newEntryInfo));
}


/**
 * @param dir current directory
 * @param currentDepth 1-based path depth
 */
FhgfsOpsErr MkFileWithPatternMsgEx::mkFile(const EntryInfo* parentInfo, MkFileDetails& mkDetails,
   EntryInfo* outEntryInfo, FileInodeStoreData& inodeDiskData)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   // reference parent
   DirInode* dir = metaStore->referenceDir(parentInfo->getEntryID(),
      parentInfo->getIsBuddyMirrored(), true);
   if ( !dir )
      return FhgfsOpsErr_PATHNOTEXISTS;

   // create meta file
   retVal = mkMetaFile(*dir, mkDetails, outEntryInfo, inodeDiskData);

   if (shouldFixTimestamps())
      fixInodeTimestamp(*dir, dirTimestamps);

   // clean-up
   metaStore->releaseDir(parentInfo->getEntryID());

   return retVal;
}

/**
 * Create an inode and directory-entry
 */
FhgfsOpsErr MkFileWithPatternMsgEx::mkMetaFile(DirInode& dir, MkFileDetails& mkDetails,
   EntryInfo* outEntryInfo, FileInodeStoreData& inodeDiskData)
{
   // note: to guarantee amtomicity of file creation (from the view of a client), we have
   //       to create the inode first and insert the directory entry afterwards

   LogContext log("MkFileWithPatternMsg::mkMetaFile");

   App* app = Program::getApp();
   TargetCapacityPools* capacityPools = app->getStorageCapacityPools();
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   // check stripe pattern

   std::unique_ptr<StripePattern> stripePattern(getPattern().clone());
   UInt16Vector stripeTargets;
   UInt16List preferredTargets(stripePattern->getStripeTargetIDs()->begin(),
      stripePattern->getStripeTargetIDs()->end() );
   unsigned numDesiredTargets = stripePattern->getDefaultNumTargets();
   unsigned minNumRequiredTargets = stripePattern->getMinNumTargets();

   // choose targets based on preference setting of given stripe pattern
   capacityPools->chooseStorageTargets(
      numDesiredTargets, minNumRequiredTargets, &preferredTargets, &stripeTargets);


   // check availability of stripe nodes
   if(stripeTargets.empty() || (stripeTargets.size() < stripePattern->getMinNumTargets() ) )
   {
      log.logErr(std::string("No (or not enough) storage targets available") );
      return FhgfsOpsErr_INTERNAL;
   }

   // swap given preferred targets of stripe pattern with chosen targets
   stripePattern->getStripeTargetIDsModifyable()->swap(stripeTargets);

   FhgfsOpsErr makeRes = metaStore->mkNewMetaFile(dir, &mkDetails, std::move(stripePattern),
      outEntryInfo, &inodeDiskData); // (note: internally deletes stripePattern)

   return makeRes;
}

bool MkFileWithPatternMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   UInt16List preferredTargets; // can be an empty dummy
   std::string newFileName = getNewFileName();

   MkFileMsg mkFileMsg(getParentInfo(), newFileName, getUserID(), getGroupID(), getMode(),
      getUmask(), &preferredTargets);

   mkFileMsg.addFlag(NetMessageHeader::Flag_BuddyMirrorSecond);
   // secondary needs to know the created entryID ans stripe pattern, because it needs to use the
   // same information
   mkFileMsg.setNewEntryID(entryID.c_str());
   mkFileMsg.setPattern(inodeDiskData.getStripePattern());
   mkFileMsg.setDirTimestamps(dirTimestamps);
   mkFileMsg.setCreateTime(inodeDiskData.getInodeStatData()->getCreationTimeSecs());

   return sendToSecondary(ctx, mkFileMsg, NETMSGTYPE_MkFileResp) == FhgfsOpsErr_SUCCESS;
}
