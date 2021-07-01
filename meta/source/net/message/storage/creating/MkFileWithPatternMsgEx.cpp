#include <common/net/message/storage/creating/MkFileMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <program/Program.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/storage/striping/StripePattern.h>
#include <common/toolkit/MathTk.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <session/EntryLock.h>
#include "MkFileWithPatternMsgEx.h"

// Called from fhgfs-ctl (online_cfg) to create a file on a specific node.

std::tuple<FileIDLock, ParentNameLock, FileIDLock> MkFileWithPatternMsgEx::lock(
      EntryLockStore& store)
{
   // no need to lock the created file as well, since
   //  a) no other operation can create the same file id
   //  b) until we finish, no part of the system except us knows the new file id
   //  c) if bulk resync gets the file while it is incomplete, individual resync will get it again
   FileIDLock dirLock(&store, getParentInfo()->getEntryID(), true);
   ParentNameLock dentryLock(&store, getParentInfo()->getEntryID(), getNewFileName());
   FileIDLock fileLock(&store, entryID, true);

   return std::make_tuple(std::move(dirLock), std::move(dentryLock), std::move(fileLock));
}

bool MkFileWithPatternMsgEx::processIncoming(ResponseContext& ctx)
{
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

   updateNodeOp(ctx, MetaOpCounter_MKFILE);

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

   std::unique_ptr<StripePattern> stripePattern(getPattern().clone());
   const UInt16Vector* stripeTargets = stripePattern->getStripeTargetIDs();
   StoragePoolId storagePoolId = stripePattern->getStoragePoolId();

   if (stripeTargets->empty())
   {
      StoragePoolPtr storagePool =
            Program::getApp()->getStoragePoolStore()->getPool(storagePoolId);

      if (!storagePool)
      {
         LOG(GENERAL, ERR, "Given Storage Pool ID "
               + StringTk::uintToStr(storagePoolId.val()) + " doesn't exist.");
         return FhgfsOpsErr_INTERNAL;
      }

      UInt16Vector chosenStripeTargets;

      if(stripePattern->getPatternType() == StripePatternType_BuddyMirror)
      {
         storagePool->getBuddyCapacityPools()->chooseStorageTargets(
               stripePattern->getDefaultNumTargets(), stripePattern->getMinNumTargets(),
               nullptr, &chosenStripeTargets);
      }
      else
      {
         storagePool->getTargetCapacityPools()->chooseStorageTargets(
               stripePattern->getDefaultNumTargets(), stripePattern->getMinNumTargets(),
               nullptr, &chosenStripeTargets);
      }
      stripePattern->getStripeTargetIDsModifyable()->swap(chosenStripeTargets);
   }

   // check if num targets and target list match and if targets actually exist
   if(stripeTargets->empty() || (stripeTargets->size() < stripePattern->getMinNumTargets() ) )
   {
      LOG(GENERAL, ERR, "No (or not enough) storage targets defined.",
               ("numTargets", stripeTargets->size()),
               ("expectedMinNumTargets", stripePattern->getMinNumTargets()));
      return FhgfsOpsErr_INTERNAL;
   }

   for (auto iter = stripeTargets->begin(); iter != stripeTargets->end(); iter++)
   {
      if(stripePattern->getPatternType() == StripePatternType_BuddyMirror)
      {
         if (!Program::getApp()->getStorageBuddyGroupMapper()->getPrimaryTargetID(*iter))
         {
            LOG(GENERAL, ERR, "Unknown buddy group targets defined.",
                     ("targetId", *iter));
            return FhgfsOpsErr_UNKNOWNTARGET;
         }
      }
      else
      {
         if (!Program::getApp()->getTargetMapper()->targetExists(*iter))
         {
            LOG(GENERAL, ERR, "Unknown storage targets defined.",
                     ("targetId", *iter));
            return FhgfsOpsErr_UNKNOWNTARGET;
         }
      }
   }

   // check if chunk size satisfies constraints
   if (!MathTk::isPowerOfTwo(stripePattern->getChunkSize()))
   {
      LOG(GENERAL, DEBUG, "Invalid chunk size: Must be a power of two.",
            stripePattern->getChunkSize());
      return FhgfsOpsErr_INTERNAL;
   }
   if (stripePattern->getChunkSize() < STRIPEPATTERN_MIN_CHUNKSIZE)
   {
      LOG(GENERAL, DEBUG, "Invalid chunk size: Below minimum size.",
               stripePattern->getChunkSize(),
               ("minChunkSize", STRIPEPATTERN_MIN_CHUNKSIZE));
      return FhgfsOpsErr_INTERNAL;
   }

   return Program::getApp()->getMetaStore()->mkNewMetaFile(
      dir, &mkDetails, std::move(stripePattern), outEntryInfo, &inodeDiskData);
      // (note: internally deletes stripePattern)
}

void MkFileWithPatternMsgEx::forwardToSecondary(ResponseContext& ctx)
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

   sendToSecondary(ctx, mkFileMsg, NETMSGTYPE_MkFileResp);
}
