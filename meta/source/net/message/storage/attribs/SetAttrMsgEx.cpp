#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>
#include <components/worker/SetChunkFileAttribsWork.h>
#include <program/Program.h>
#include <session/EntryLock.h>
#include "SetAttrMsgEx.h"

#include <boost/lexical_cast.hpp>


bool SetAttrMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "SetAttrMsg incoming";
#endif // BEEGFS_DEBUG

   EntryInfo* entryInfo = getEntryInfo();

   LOG_DEBUG(logContext, Log_DEBUG,
      "BuddyMirrored: " + std::string(entryInfo->getIsBuddyMirrored() ? "Yes" : "No") +
      " Secondary: " + std::string(hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond)
         ? "Yes" : "No") );
   (void) entryInfo;

   // update operation counters (here on top because we have an early sock release in this msg)
   updateNodeOp(ctx, MetaOpCounter_SETATTR);

   return BaseType::processIncoming(ctx);
}

std::tuple<FileIDLock, FileIDLock> SetAttrMsgEx::lock(EntryLockStore& store)
{
   if (DirEntryType_ISDIR(getEntryInfo()->getEntryType()))
      return std::make_tuple(
            FileIDLock(),
            FileIDLock(&store, getEntryInfo()->getEntryID(), true));
   else
      return std::make_tuple(
            FileIDLock(&store, getEntryInfo()->getEntryID(), true),
            FileIDLock());
}

std::unique_ptr<MirroredMessageResponseState> SetAttrMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   const char* logContext = "Set file attribs";

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   Config* cfg = app->getConfig();

   EntryInfo* entryInfo = getEntryInfo();
   FhgfsOpsErr setAttrRes;
   const bool forwardToStorage = !entryInfo->getIsBuddyMirrored()
         || !hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond);

   bool responseSent = false;

   if (entryInfo->getParentEntryID().empty() || DirEntryType_ISDIR(entryInfo->getEntryType()))
   {
      // special case: setAttr for root directory
      if (entryInfo->getParentEntryID().empty())
         setAttrRes = setAttrRoot();
      else
         setAttrRes = metaStore->setAttr(entryInfo, getValidAttribs(), getAttribs());

      if (setAttrRes != FhgfsOpsErr_SUCCESS || !(shouldFixTimestamps() || getFileEvent()))
         return boost::make_unique<ResponseState>(setAttrRes);

      if (shouldFixTimestamps())
      {
         auto dir = metaStore->referenceDir(entryInfo->getEntryID(),
               entryInfo->getIsBuddyMirrored(), true);
         if (dir)
         {
            fixInodeTimestamp(*dir, inodeTimestamps);
            metaStore->releaseDir(dir->getID());
         }
      }

      if (!isSecondary && getFileEvent() && app->getFileEventLogger() && getFileEvent())
      {
            app->getFileEventLogger()->log(
                     *getFileEvent(),
                     entryInfo->getEntryID(),
                     entryInfo->getParentEntryID());
      }

      return boost::make_unique<ResponseState>(setAttrRes);
   }

   // update nlink count if requested by caller
   if (isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_INCR_NLINKCNT) &&
      isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_DECR_NLINKCNT))
   {
      // error: both increase and decrease of nlink count was requested
      return boost::make_unique<ResponseState>(FhgfsOpsErr_INTERNAL);
   }
   else if (isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_INCR_NLINKCNT) ||
      isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_DECR_NLINKCNT))
   {
      int val = isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_INCR_NLINKCNT) ? 1 : -1;
      setAttrRes = metaStore->incDecLinkCount(entryInfo, val);
      return boost::make_unique<ResponseState>(setAttrRes);
   }

   // we need to reference the inode first, as we want to use it several times
   MetaFileHandle inode = metaStore->referenceFile(entryInfo);
   if (!inode)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   // in the following we need to distinguish between several cases.
   // 1. if times shall be updated we need to send the update to the storage servers first
   //    because, we need to rely on storage server's attrib version to prevent races with
   //    other messages that update the times (e.g. Close)
   // 2. if times shall not be updated (must be chmod or chown then) and quota is enabled
   //    we first set the local attributes and then send the update to the storage server.
   //    if an early response optimization is set in this case we send the response between
   //    these two steps
   // 3. no times update (i.e. chmod or chown) and quota is disabled => only update locally,
   //    as we don't have a reason to waste time with contacting the storage servers

   bool timeUpdate =
      getValidAttribs() & (SETATTR_CHANGE_MODIFICATIONTIME | SETATTR_CHANGE_LASTACCESSTIME);

   // note: time update and mode/owner update can never be at the same time
   if (timeUpdate && forwardToStorage)
   {
      // only relevant if message needs to be sent to the storage server. if not (e.g.
      // because this is secondary of a buddy group, we can use the "default" case later
      setAttrRes = setChunkFileAttribs(*inode, true);

      if (setAttrRes == FhgfsOpsErr_SUCCESS)
         setAttrRes = metaStore->setAttr(entryInfo, getValidAttribs(), getAttribs());
   }
   else if (isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_USE_QUOTA) && forwardToStorage)
   {
      const UInt16Vector* targetIdVec = inode->getStripePattern()->getStripeTargetIDs();
      ExceededQuotaStorePtr quotaExStore;
      for (auto targetIter = targetIdVec->begin(); targetIter != targetIdVec->end(); targetIter++)
      {
         if (inode->getStripePattern()->getPatternType() == StripePatternType_BuddyMirror)
         {
            uint16_t primaryTargetID = app->getStorageBuddyGroupMapper()->getPrimaryTargetID(*targetIter);
            quotaExStore = app->getExceededQuotaStores()->get(primaryTargetID);
         }
         else
         {
            // this is not very efficient at the moment, as we need to look at every quota exceeded
            // store for every target in the stripe pattern; we need to do that because storage pools
            // might change over time, i.e. the pool that is stored in the metadata doesn't always
            // match the actual targets a file is stored on
            quotaExStore = app->getExceededQuotaStores()->get(*targetIter);
         }

         // check if exceeded quotas exists, before doing a more expensive and explicit check
         if (quotaExStore && quotaExStore->someQuotaExceeded())
         { // store for this target is present AND someQuotaExceeded() was true
            QuotaExceededErrorType quotaExceeded = quotaExStore->isQuotaExceeded(
               getAttribs()->userID, getAttribs()->groupID);

            if (quotaExceeded != QuotaExceededErrorType_NOT_EXCEEDED)
            {
               LogContext(logContext).log(Log_NOTICE,
                  QuotaData::QuotaExceededErrorTypeToString(quotaExceeded) + " "
                     "UID: " + StringTk::uintToStr(getAttribs()->userID) + "; "
                     "GID: " + StringTk::uintToStr(getAttribs()->groupID));

               setAttrRes = FhgfsOpsErr_DQUOT;
               goto finish;
            }
         }
      }

      // only relevant if message needs to be sent to the storage server. if not (e.g.
      // because this is secondary of a buddy group, we can use the "default" case later
      setAttrRes = metaStore->setAttr(entryInfo, getValidAttribs(), getAttribs());

      // allowed only if early chown respnse for quota is set
      if (cfg->getQuotaEarlyChownResponse())
      {
         earlyComplete(ctx, ResponseState(setAttrRes));
         responseSent = true;
      }

      if (setAttrRes == FhgfsOpsErr_SUCCESS)
         setChunkFileAttribs(*inode, false);
   }
   else
   {
      setAttrRes = metaStore->setAttr(entryInfo, getValidAttribs(), getAttribs());
   }

finish:
   if (shouldFixTimestamps())
      fixInodeTimestamp(*inode, inodeTimestamps, entryInfo);

   if (!isSecondary && setAttrRes == FhgfsOpsErr_SUCCESS &&
         app->getFileEventLogger() && getFileEvent())
   {
         app->getFileEventLogger()->log(
                  *getFileEvent(),
                  entryInfo->getEntryID(),
                  entryInfo->getParentEntryID(),
                  "",
                  inode->getNumHardlinks() > 1);
   }

   metaStore->releaseFile(entryInfo->getParentEntryID(), inode);

   if (!responseSent)
      return boost::make_unique<ResponseState>(setAttrRes);
   else
      return {};
}

void SetAttrMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_SetAttrResp);
}

FhgfsOpsErr SetAttrMsgEx::setAttrRoot()
{
   App* app = Program::getApp();
   DirInode* rootDir = app->getRootDir();

   NumNodeID expectedOwnerNode = rootDir->getIsBuddyMirrored()
      ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() )
      : app->getLocalNode().getNumID();

   if ( expectedOwnerNode != rootDir->getOwnerNodeID() )
      return FhgfsOpsErr_NOTOWNER;

   if(!rootDir->setAttrData(getValidAttribs(), getAttribs() ) )
      return FhgfsOpsErr_INTERNAL;


   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr SetAttrMsgEx::setChunkFileAttribs(FileInode& file, bool requestDynamicAttribs)
{
   StripePattern* pattern = file.getStripePattern();

   if( (pattern->getStripeTargetIDs()->size() > 1) ||
       (pattern->getPatternType() == StripePatternType_BuddyMirror) )
      return setChunkFileAttribsParallel(file, requestDynamicAttribs);
   else
      return setChunkFileAttribsSequential(file, requestDynamicAttribs);
}

/**
 * Note: This method does not work for mirrored files; use setChunkFileAttribsParallel() for those.
 */
FhgfsOpsErr SetAttrMsgEx::setChunkFileAttribsSequential(FileInode& inode,
   bool requestDynamicAttribs)
{
   const char* logContext = "Set chunk file attribs S";

   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   TargetStateStore* targetStates = Program::getApp()->getTargetStateStore();
   NodeStore* nodes = Program::getApp()->getStorageNodes();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   std::string fileID(inode.getEntryID());

   PathInfo pathInfo;
   inode.getPathInfo(&pathInfo);

   // send request to each node and receive the response message
   unsigned currentTargetIndex = 0;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      iter != targetIDs->end();
      iter++, currentTargetIndex++)
   {
      uint16_t targetID = *iter;
      bool enableFileCreation = (currentTargetIndex == 0); // enable inode creation of first node

      SetLocalAttrMsg setAttrMsg(fileID, targetID, &pathInfo, getValidAttribs(), getAttribs(),
         enableFileCreation);

      setAttrMsg.setMsgHeaderUserID(inode.getUserID());

      if(isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_USE_QUOTA))
         setAttrMsg.addMsgHeaderFeatureFlag(SETLOCALATTRMSG_FLAG_USE_QUOTA);

      RequestResponseArgs rrArgs(NULL, &setAttrMsg, NETMSGTYPE_SetLocalAttrResp);
      RequestResponseTarget rrTarget(targetID, targetMapper, nodes);

      rrTarget.setTargetStates(targetStates);

      // send request to node and receive response
      FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

      if(requestRes != FhgfsOpsErr_SUCCESS)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with storage target failed: " + StringTk::uintToStr(targetID) + "; "
            "fileID: " + inode.getEntryID() + "; "
            "Error: " + boost::lexical_cast<std::string>(requestRes));

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = requestRes;

         continue;
      }

      // correct response type received
      const auto setRespMsg = (const SetLocalAttrRespMsg*)rrArgs.outRespMsg.get();

      FhgfsOpsErr setRespResult = setRespMsg->getResult();
      if (setRespResult != FhgfsOpsErr_SUCCESS)
      { // error: local inode attribs not set
         LogContext(logContext).log(Log_WARNING,
            "Target failed to set attribs of chunk file: " + StringTk::uintToStr(targetID) + "; "
            "fileID: " + inode.getEntryID());

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = setRespResult;

         continue;
      }

      // success: local inode attribs set
      if (setRespMsg->isMsgHeaderFeatureFlagSet(SETLOCALATTRRESPMSG_FLAG_HAS_ATTRS))
      {
         DynamicFileAttribsVec dynAttribsVec(1);
         setRespMsg->getDynamicAttribs(&(dynAttribsVec[0]));
         inode.setDynAttribs(dynAttribsVec);
      }

      LOG_DEBUG(logContext, Log_DEBUG,
         "Target has set attribs of chunk file: " + StringTk::uintToStr(targetID) + "; " +
         "fileID: " + inode.getEntryID());
   }

   if(unlikely(retVal != FhgfsOpsErr_SUCCESS) )
      LogContext(logContext).log(Log_WARNING,
         "Problems occurred during setting of chunk file attribs. "
         "fileID: " + inode.getEntryID());

   return retVal;
}

FhgfsOpsErr SetAttrMsgEx::setChunkFileAttribsParallel(FileInode& inode, bool requestDynamicAttribs)
{
   const char* logContext = "Set chunk file attribs";

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();

   size_t numTargetWorks = targetIDs->size();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   DynamicFileAttribsVec dynAttribsVec(numTargetWorks);
   FhgfsOpsErrVec nodeResults(numTargetWorks);
   SynchronizedCounter counter;

   PathInfo pathInfo;
   inode.getPathInfo(&pathInfo);

   // generate work for storage targets...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      bool enableFileCreation = (i == 0); // enable inode creation on first target

      SetChunkFileAttribsWork* work = NULL;
      if (requestDynamicAttribs) // we are interested in the chunk's dynamic attributes, because we
      {                          // modify timestamps and this operation might race with others
         work = new SetChunkFileAttribsWork(inode.getEntryID(), getValidAttribs(), getAttribs(),
            enableFileCreation, pattern, (*targetIDs)[i], &pathInfo, &(dynAttribsVec[i]),
            &(nodeResults[i]), &counter);
      }
      else
      {
         work = new SetChunkFileAttribsWork(inode.getEntryID(), getValidAttribs(), getAttribs(),
            enableFileCreation, pattern, (*targetIDs)[i], &pathInfo, NULL, &(nodeResults[i]),
            &counter);
      }

      work->setQuotaChown(isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_USE_QUOTA) );
      work->setMsgUserID(getMsgHeaderUserID() );

      slaveQ->addDirectWork(work);
   }

   // wait for work completion...
   counter.waitForCount(numTargetWorks);

   // we set the dynamic attribs here, no matter if the remote operation suceeded or not. If it
   // did not, storageVersion will be zero and the corresponding data will be ignored
   // note: if the chunk's attributes were not requested from server at all, this is also OK here,
   // because the storageVersion will be 0
   inode.setDynAttribs(dynAttribsVec);

   // check target results...
   for(size_t i=0; i < numTargetWorks; i++)
   {
      if(unlikely(nodeResults[i] != FhgfsOpsErr_SUCCESS) )
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during setting of chunk file attribs. "
            "fileID: " + inode.getEntryID());

         retVal = nodeResults[i];
         goto error_exit;
      }
   }


error_exit:
   return retVal;
}
