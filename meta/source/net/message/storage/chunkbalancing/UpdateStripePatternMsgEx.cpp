#include <toolkit/StorageTkEx.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include <components/FileEventLogger.h>


#include "UpdateStripePatternMsgEx.h"

FileIDLock UpdateStripePatternMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

bool  UpdateStripePatternMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
   const char* logContext = "UpdateStripePatternMsg incoming";
   #endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_SPAM, "Attempting to change stripe pattern of file at chunkPath: " + getRelativePath() + "; from localTargetID: "
         + std::to_string(getTargetID()) + "; to destinationTargetID: "
         + std::to_string(getDestinationID()));
   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState>  UpdateStripePatternMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   uint16_t targetID = getTargetID();
   uint16_t destinationID = getDestinationID();
   std::string& relativePath = getRelativePath();
   EntryInfo* entryInfo = getEntryInfo();
   std::string entryID = entryInfo->getEntryID();
   FhgfsOpsErr stripePatternMsgRes;
   const char* logContext = "Update Stripe Pattern";
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   bool changeStripePatternRes = false;
   bool shouldUpdateStripePattern = false;
   std::string entryIDfromPath = StorageTk::getPathBasename(relativePath);
   FileEventLogger* eventLogger = app->getFileEventLogger();
   FhgfsOpsErr storageResyncRes = getResyncResult();


   FileEvent* fileEvent = getFileEvent(); //create and initialize FileEvent object
   if (fileEvent){
      fileEvent->type = FileEventType::STRIPE_PATTERN_CHANGED;
   }

   // Validate entry ID
   if (entryIDfromPath != entryID)
   {
      // Log error and return
      return boost::make_unique<ResponseState>(FhgfsOpsErr_INTERNAL);
   }

   // Handle secondary case
   if (isSecondary)
   {

      if (unlikely(storageResyncRes != FhgfsOpsErr_SUCCESS  && storageResyncRes != FhgfsOpsErr_PATHNOTEXISTS))
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Storage was unable to copy chunk to destination. Stripe pattern will not be updated on secondary. ");
         stripePatternMsgRes = FhgfsOpsErr_INTERNAL;
         return boost::make_unique<ResponseState>(stripePatternMsgRes);
      }

      FileInode* inode = FileInode::createFromEntryInfo(entryInfo);
      if (!inode)
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Unable to create inode and update stripe pattern on secondary buddy. Ensure a buddy resync operation is started to update stripe pattern on secondary. ");
         stripePatternMsgRes = FhgfsOpsErr_INTERNAL;
         return boost::make_unique<ResponseState>(stripePatternMsgRes);
      }
      //storageResyncRes is either PATHNOTEXISTS or SUCCESS
      if (storageResyncRes == FhgfsOpsErr_PATHNOTEXISTS) //chunk does not exist on storage, check file size to see if file size is smaller than expected
      {
         shouldUpdateStripePattern = checkChunkOnStorageTarget(*inode, relativePath, targetID);
         if (!shouldUpdateStripePattern)
         {
            stripePatternMsgRes = storageResyncRes; //set the error result from resync operation and notify storage
            return boost::make_unique<ResponseState>(stripePatternMsgRes);
         }
      }
      //storageResyncRes is SUCCESS
      bool changeStripePatternRes = setStripePattern(entryInfo, *inode,  relativePath, targetID, destinationID);

      EventContext eventCtx = makeEventContext(     //setup event context
         entryInfo,
         entryInfo->getParentEntryID(),
         getMsgHeaderUserID(),
         "",
         inode ? inode->getNumHardlinks() : 0,
         isSecondary
      );
      SAFE_DELETE(inode);
      if (!changeStripePatternRes)
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Unable to change chunk striping pattern on secondary buddy. Ensure a buddy resync operation is started to update stripe pattern on secondary. ");
         stripePatternMsgRes = FhgfsOpsErr_INTERNAL;
         return boost::make_unique<ResponseState>(stripePatternMsgRes);
      }
      if (eventLogger && fileEvent) //if event logger is available and fileEvent is valid
      {
         logEvent(eventLogger , *fileEvent, eventCtx); //notify event listener of stripe pattern change on secondary
      }
      stripePatternMsgRes = FhgfsOpsErr_SUCCESS;
      return boost::make_unique<ResponseState>(stripePatternMsgRes);
  }

   // Handle primary case
   GlobalInodeLockStore* inodeLockStore = metaStore->getInodeLockStore();
   if (!inodeLockStore)
   {
      LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Unable to obtain GlobalInodeLockStore. There may be orphaned chunks in the system, you may want to perform a filesystem check to ensure all additional chunks are deleted. ");
      stripePatternMsgRes = FhgfsOpsErr_INTERNAL;
      return boost::make_unique<ResponseState>(stripePatternMsgRes);
   }

   //refCount already increased by ChunkBalanceMetaSlave so no need to increase it again
   FileInode* inode = inodeLockStore->getFileInodeUnreferenced(entryInfo);
   if (!inode)
   {
      LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Unable to obtain file inode. There may be orphaned chunks in the system, you may want to perform a filesystem check to ensure all additional chunks are deleted. ");
      stripePatternMsgRes = FhgfsOpsErr_INTERNAL;
      return boost::make_unique<ResponseState>(stripePatternMsgRes);
   }

   if (storageResyncRes != FhgfsOpsErr_SUCCESS  && storageResyncRes != FhgfsOpsErr_PATHNOTEXISTS)  //check result of chunk resync
   {
      LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Storage was unable to copy chunk to destination. Releasing file from lock store. chunkPath: "
      + relativePath + "; localTargetID: "
      + std::to_string(targetID) + "; destinationTargetID: "
      + std::to_string(destinationID));

      inodeLockStore->releaseFileInode(entryID, LockOperationType::CHUNK_REBALANCING); //release access to the file inode on primary without updating stripe pattern
      stripePatternMsgRes = storageResyncRes; //set the error result from resync operation and notify storage
      return boost::make_unique<ResponseState>(stripePatternMsgRes);
   }
   //storageResyncRes is either PATHNOTEXISTS or SUCCESS
   if (storageResyncRes == FhgfsOpsErr_PATHNOTEXISTS) //chunk does not exist on storage, check file size to see if file size is smaller than expected
   {
      shouldUpdateStripePattern = checkChunkOnStorageTarget(*inode, relativePath, targetID);
      if (!shouldUpdateStripePattern)
      {
         inodeLockStore->releaseFileInode(entryID, LockOperationType::CHUNK_REBALANCING); //release access to the file inode on primary without updating stripe pattern
         stripePatternMsgRes = storageResyncRes; //set the error result from resync operation and notify storage
         return boost::make_unique<ResponseState>(stripePatternMsgRes);
      }
   }
   //storageResyncRes is SUCCESS
   changeStripePatternRes = setStripePattern(entryInfo, *inode,  relativePath, targetID, destinationID);
   if (!changeStripePatternRes)
   {
      LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Unable to change chunk striping pattern. There may be orphaned chunks in the system, you may want to perform a filesystem check to ensure all additional chunks are deleted. ");
      stripePatternMsgRes = FhgfsOpsErr_INTERNAL;
   }
   else
   {
      stripePatternMsgRes = FhgfsOpsErr_SUCCESS;
   }

   EventContext eventCtx = makeEventContext(
      entryInfo,
      entryInfo->getParentEntryID(),
      getMsgHeaderUserID(),
      "",
      inode ? inode->getNumHardlinks() : 0,
      isSecondary
   );
   if (eventLogger && fileEvent) //if event logger is available and fileEvent is valid
   {
      logEvent(eventLogger , *fileEvent, eventCtx); //notify event listener of stripe pattern change on primary
   }
   inodeLockStore->releaseFileInode(entryID, LockOperationType::CHUNK_REBALANCING); //release access to the file inode on primary


   LOG_DEBUG(logContext, Log_SPAM,  "Successfully changed stripe pattern of chunk at chunkPath: " + relativePath + "; entryID: "
         + entryID + "; localTargetID: "
         + std::to_string(targetID) + "; destinationTargetID: "
         + std::to_string(destinationID));

   return boost::make_unique<ResponseState>(stripePatternMsgRes);
}
bool UpdateStripePatternMsgEx::setStripePattern(EntryInfo* entryInfo, FileInode& inode, std::string& relativePath, uint16_t localTargetID, uint16_t destinationID)
{
   LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_SPAM,
      "Change chunk stripe pattern operation started. chunkPath: " + relativePath + "; localTargetID: "
      + std::to_string(localTargetID) + "; destinationTargetID: "
      + std::to_string(destinationID));
   bool setPatternRes = inode.modifyStripePattern(localTargetID, destinationID);
   if (!setPatternRes)
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_WARNING,
         "Failed to update stripe pattern: chunkPath: " + relativePath + "; localTargetID: "
            + std::to_string(localTargetID) + "; destinationTargetID: "
            + std::to_string(destinationID));
      return setPatternRes;
   }

   setPatternRes = inode.updateInodeOnDiskIncrementVersion(entryInfo, inode.getStripePattern());
   if (!setPatternRes)
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_WARNING,
      "Failed to update stripe pattern on disk: chunkPath: " + relativePath + "; localTargetID: "
         + std::to_string(localTargetID) + "; destinationTargetID: "
         + std::to_string(destinationID));
   }

   return setPatternRes;
}

void UpdateStripePatternMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_UpdateStripePatternResp);
}


bool UpdateStripePatternMsgEx::checkChunkOnStorageTarget(FileInode& inode, std::string& relativePath, uint16_t targetID)
{
   StatData statData;
   FhgfsOpsErr statRes    = inode.getStatData(statData);
   if (statRes != FhgfsOpsErr_SUCCESS)
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Storage was unable to copy chunk to destination and unable to get file size. Releasing file from lock store. chunkPath: "
         + relativePath + "; localTargetID: "
         + std::to_string(targetID));
      return false; //do not update stripe pattern
   }

   int64_t fileSize = statData.getFileSize();
   bool targetActiveRes = inode.checkTargetIsActiveInPattern(fileSize, targetID);

   if (targetActiveRes)  //check if file is empty, symlink or otherwise smaller than expected
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_DEBUG, "Storage was unable to copy chunk to destination, file does not contain data on this target. "
         "Likely file is smaller than the stripe width. Proceeding to update stripe pattern anyway. chunkPath: "
         + relativePath + "; localTargetID: " + std::to_string(targetID));
      return true;  //continue to update stripe pattern
   }
   else
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_ERR, "Unexpected error. Storage was unable to copy chunk to destination but chunk should be on the target. Releasing file from lock store. chunkPath: "
         + relativePath + "; localTargetID: "+ std::to_string(targetID));
      return false; //do not update stripe pattern
   }
}