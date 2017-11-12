#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include "TruncLocalFileMsgEx.h"


#define TRUNCLOCALFILE_CHUNKOPENLAGS (O_CREAT|O_WRONLY|O_LARGEFILE)


bool TruncLocalFileMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "TruncChunkFileMsg incoming";

   #ifdef BEEGFS_DEBUG
      LOG_DEBUG(logContext, Log_DEBUG, "Received a TruncLocalFileMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

   App* app = Program::getApp();

   uint16_t targetID;
   int targetFD;
   bool chunkLocked = false;
   FhgfsOpsErr clientErrRes;
   DynamicAttribs dynAttribs; // inits storageVersion to 0 (=> initially invalid)


   // select the right targetID

   targetID = getTargetID();

   if(isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR) )
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) ?
         mirrorBuddies->getSecondaryTargetID(targetID) :
         mirrorBuddies->getPrimaryTargetID(targetID);

      if(unlikely(!targetID) )
      { // unknown group ID
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            StringTk::uintToStr(getTargetID() ) );
         clientErrRes = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }
   }


   { // get targetFD and check consistency state
      bool skipResponse = false;

      targetFD = getTargetFD(ctx, targetID, &skipResponse);
      if(unlikely(targetFD == -1) )
      { // failed => either unknown targetID or consistency state not good
         if(skipResponse)
            goto skip_response; // GenericResponseMsg sent

         clientErrRes = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }
   }

   // forward to secondary (if appropriate)
   clientErrRes = forwardToSecondary(ctx, targetID, &chunkLocked);
   if(unlikely(clientErrRes != FhgfsOpsErr_SUCCESS) )
   {
      if(clientErrRes == FhgfsOpsErr_COMMUNICATION)
         goto skip_response; // GenericResponseMsg sent

      goto send_response;
   }

   { // valid targetID
      std::string entryID(getEntryID() );

      // generate path to chunk file...

      Path chunkDirPath;
      std::string chunkFilePathStr;
      const PathInfo *pathInfo = getPathInfo();
      bool hasOrigFeature = pathInfo->hasOrigFeature();

      StorageTk::getChunkDirChunkFilePath(pathInfo, entryID, hasOrigFeature, chunkDirPath,
         chunkFilePathStr);

      // truncate file...

      clientErrRes = truncFile(targetID, targetFD, &chunkDirPath, chunkFilePathStr, entryID,
         hasOrigFeature);

      /* clientErrRes == FhgfsOpsErr_PATHNOTEXISTS && !getFileSize() is special we need to fake
       * the attributes, to inform the metaserver about the new file size with storageVersion!=0 */
      if(clientErrRes == FhgfsOpsErr_SUCCESS ||
         (clientErrRes == FhgfsOpsErr_PATHNOTEXISTS && !getFilesize() ) )
      { // truncation successful
         LOG_DEBUG(logContext, Log_DEBUG, "File truncated: " + chunkFilePathStr);

         // get updated dynamic attribs...

         if(!isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_NODYNAMICATTRIBS) )
         {
            if (clientErrRes == FhgfsOpsErr_SUCCESS)
               getDynamicAttribsByPath(targetFD, chunkFilePathStr.c_str(), targetID, entryID,
                  dynAttribs);
            else
            { // clientErrRes == FhgfsOpsErr_PATHNOTEXISTS && !getFileSize()
               getFakeDynAttribs(targetID, entryID, dynAttribs);
            }
         }

         // change to SUCCESS if it was FhgfsOpsErr_PATHNOTEXISTS
         clientErrRes = FhgfsOpsErr_SUCCESS;
      }
   }


send_response:

   if(chunkLocked) // unlock chunk
      app->getChunkLockStore()->unlockChunk(targetID, getEntryID() );

   // send response...
   ctx.sendResponse(
         TruncLocalFileRespMsg(clientErrRes, dynAttribs.filesize, dynAttribs.allocedBlocks,
            dynAttribs.modificationTimeSecs, dynAttribs.lastAccessTimeSecs,
            dynAttribs.storageVersion) );

skip_response:

   // update operation counters
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      StorageOpCounter_TRUNCLOCALFILE, getMsgHeaderUserID() );

   return true;
}

/**
 * @param outResponseSent true if a response was sent from within this method; can only be true if
 * -1 is returned.
 * @return -1 if no such target exists or if consistency state was not good (in which case a special
 * response is sent within this method), otherwise the file descriptor to chunks dir (or mirror
 * dir).
 */
int TruncLocalFileMsgEx::getTargetFD(ResponseContext& ctx, uint16_t actualTargetID,
   bool* outResponseSent)
{
   const char* logContext = "TruncChunkFileMsg (get target FD)";

   App* app = Program::getApp();

   bool isBuddyMirrorChunk = isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR);
   TargetConsistencyState consistencyState = TargetConsistencyState_BAD; // silence gcc

   *outResponseSent = false;

   // get targetFD and check consistency state

   int targetFD = app->getTargetFDAndConsistencyState(actualTargetID, isBuddyMirrorChunk,
      &consistencyState);

   if(unlikely(targetFD == -1) )
   { // unknown targetID
      if(isBuddyMirrorChunk)
      { /* buddy mirrored file => fail with GenericResp to make the caller retry.
           mgmt will mark this target as (p)offline in a few moments. */
         std::string respMsgLogStr = "Refusing request. "
            "Unknown targetID: " + StringTk::uintToStr(actualTargetID);

         ctx.sendResponse(
               GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR, respMsgLogStr.c_str() ) );

         *outResponseSent = true;
         return -1;
      }

      LogContext(logContext).logErr("Unknown targetID: " + StringTk::uintToStr(actualTargetID) );

      return -1;
   }

   if(unlikely(consistencyState != TargetConsistencyState_GOOD) &&
      isBuddyMirrorChunk &&
      !isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) )
   { // this is a msg to a non-good primary
      std::string respMsgLogStr = "Refusing request. Target consistency is not good. "
         "targetID: " + StringTk::uintToStr(actualTargetID);

      ctx.sendResponse(
            GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR, respMsgLogStr.c_str() ) );

      *outResponseSent = true;
      return -1;
   }

   return targetFD;
}

FhgfsOpsErr TruncLocalFileMsgEx::truncFile(uint16_t targetId, int targetFD,
   const Path* chunkDirPath, const std::string& chunkFilePathStr, std::string entryID,
   bool hasOrigFeature)
{
   const char* logContext = "TruncLocalFileMsg incoming";
   App* app = Program::getApp();

   FhgfsOpsErr clientErrRes = FhgfsOpsErr_SUCCESS;

   int truncRes = MsgHelperIO::truncateAt(targetFD, chunkFilePathStr.c_str(), getFilesize() );
   if(!truncRes)
      return FhgfsOpsErr_SUCCESS; // truncate succeeded

   // file or path just doesn't exist or real error?

   int truncErrCode = errno;

   if(unlikely(truncErrCode != ENOENT) )
   { // error
      clientErrRes = FhgfsOpsErrTk::fromSysErr(truncErrCode);
      if (clientErrRes == FhgfsOpsErr_INTERNAL) // only log unhandled errors
         LogContext(logContext).logErr("Unable to truncate file: " + chunkFilePathStr + ". " +
            "SysErr: " + System::getErrString(truncErrCode) );

      return clientErrRes;
   }

   // ENOENT => file (and possibly path to file (dirs) ) doesn't exist

   /* note: if the file doesn't exist, it's generally not an error.
      but if it should grow to a certain size, we have to create it... */

   if(!getFilesize() )
      return FhgfsOpsErr_PATHNOTEXISTS; // nothing to be done

   // create the file and re-size it

   bool useQuota = isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_USE_QUOTA);
   bool enforceQuota = app->getConfig()->getQuotaEnableEnforcement();
   SessionQuotaInfo quotaInfo(useQuota, enforceQuota, getUserID(), getGroupID());
   const ExceededQuotaStorePtr exceededQuotaStore = app->getExceededQuotaStores()->get(targetId);

   ChunkStore* chunkDirStore = app->getChunkDirStore();
   int fd;
   int openFlags = TRUNCLOCALFILE_CHUNKOPENLAGS;

   FhgfsOpsErr mkChunkRes = chunkDirStore->openChunkFile(targetFD, chunkDirPath, chunkFilePathStr,
      hasOrigFeature, openFlags, &fd, &quotaInfo, exceededQuotaStore);

   if (unlikely(mkChunkRes == FhgfsOpsErr_NOTOWNER && useQuota) )
   {
      // it already logs a message, so need to further check this ret value
      chunkDirStore->chmodV2ChunkDirPath(targetFD, chunkDirPath, entryID);

      mkChunkRes = chunkDirStore->openChunkFile(
         targetFD, chunkDirPath, chunkFilePathStr, hasOrigFeature, openFlags, &fd, &quotaInfo,
         exceededQuotaStore);
   }

   if (mkChunkRes != FhgfsOpsErr_SUCCESS)
   {
      if (mkChunkRes == FhgfsOpsErr_INTERNAL) // only log unhandled errors
         LogContext(logContext).logErr("Failed to create chunkFile: " + chunkFilePathStr);

      return mkChunkRes;
   }

   // file created => trunc it

   int ftruncRes = ftruncate(fd, getFilesize() );
   if(unlikely(ftruncRes == -1) )
   { // error
      clientErrRes = FhgfsOpsErrTk::fromSysErr(errno);
      if (clientErrRes == FhgfsOpsErr_INTERNAL) // only log unhandled errors
         LogContext(logContext).logErr(
            "Unable to truncate file (after creation): " + chunkFilePathStr + ". " +
            "Length: " + StringTk::int64ToStr(getFilesize() ) + ". " +
            "SysErr: " + System::getErrString() );
   }

   // close file

   int closeRes = close(fd);
   if(unlikely(closeRes == -1) )
   { // error
      clientErrRes = FhgfsOpsErrTk::fromSysErr(errno);
      if (clientErrRes == FhgfsOpsErr_INTERNAL) // only log unhandled errors
         LogContext(logContext).logErr(
            "Unable to close file (after creation/truncation): " + chunkFilePathStr + ". " +
            "Length: " + StringTk::int64ToStr(getFilesize() ) + ". " +
            "SysErr: " + System::getErrString() );
   }


   return clientErrRes;
}

bool TruncLocalFileMsgEx::getDynamicAttribsByPath(const int dirFD, const char* path,
   uint16_t targetID, std::string fileID, DynamicAttribs& outDynAttribs)
{
   SyncedStoragePaths* syncedPaths = Program::getApp()->getSyncedStoragePaths();

   uint64_t storageVersion = syncedPaths->lockPath(fileID, targetID); // L O C K path

   // note: this is locked because we need to get the filesize together with the storageVersion
   bool getDynAttribsRes = StorageTkEx::getDynamicFileAttribs(dirFD, path,
      &outDynAttribs.filesize, &outDynAttribs.allocedBlocks, &outDynAttribs.modificationTimeSecs,
      &outDynAttribs.lastAccessTimeSecs);

   if(getDynAttribsRes)
      outDynAttribs.storageVersion = storageVersion;

   syncedPaths->unlockPath(fileID, targetID); // U N L O C K path

   return getDynAttribsRes;
}

/**
 * Note: only for fileSize == 0 and if the file does not exist yet
 */
bool TruncLocalFileMsgEx::getFakeDynAttribs(uint16_t targetID, std::string fileID,
   DynamicAttribs& outDynAttribs)
{
   SyncedStoragePaths* syncedPaths = Program::getApp()->getSyncedStoragePaths();
   uint64_t storageVersion = syncedPaths->lockPath(fileID, targetID); // L O C K path

   int64_t currentTimeSecs = TimeAbs().getTimeval()->tv_sec;

   outDynAttribs.filesize             = 0;
   outDynAttribs.allocedBlocks        = 0;
   outDynAttribs.modificationTimeSecs = currentTimeSecs;
   outDynAttribs.lastAccessTimeSecs   = currentTimeSecs; /* actually not correct, but better than
                                                          * 1970 */
   outDynAttribs.storageVersion       = storageVersion;

   syncedPaths->unlockPath(fileID, targetID); // U N L O C K path

   return true;
}

/**
 * If this is a buddy mirror msg and we are the primary, forward this msg to secondary.
 *
 * @return _COMMUNICATION if forwarding to buddy failed and buddy is not marked offline (in which
 *    case *outChunkLocked==false is guaranteed).
 * @throw SocketException if sending of GenericResponseMsg fails.
 */
FhgfsOpsErr TruncLocalFileMsgEx::forwardToSecondary(ResponseContext& ctx, uint16_t actualTargetID,
   bool* outChunkLocked)
{
   const char* logContext = "TruncLocalFileMsgEx incoming (forward to secondary)";

   App* app = Program::getApp();
   ChunkLockStore* chunkLockStore = app->getChunkLockStore();
   StorageTargets* storageTargets = app->getStorageTargets();

   *outChunkLocked = false;

   if(!isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR) ||
       isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) )
      return FhgfsOpsErr_SUCCESS; // nothing to do

   // mirrored chunk should be modified, check if resync is in progress and lock chunk
   *outChunkLocked = storageTargets->isBuddyResyncInProgress(actualTargetID);
   if(*outChunkLocked)
      chunkLockStore->lockChunk(actualTargetID, getEntryID() ); // lock chunk

   // instead of creating a new msg object, we just re-use "this" with "buddymirror second" flag
   addMsgHeaderFeatureFlag(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);

   RequestResponseArgs rrArgs(NULL, this, NETMSGTYPE_TruncLocalFileResp);
   RequestResponseTarget rrTarget(getTargetID(), app->getTargetMapper(), app->getStorageNodes(),
      app->getTargetStateStore(), app->getMirrorBuddyGroupMapper(), true);

   FhgfsOpsErr commRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

   // remove the flag that we just added for secondary
   unsetMsgHeaderFeatureFlag(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);

   if(unlikely(
      (commRes == FhgfsOpsErr_COMMUNICATION) &&
      (rrTarget.outTargetReachabilityState == TargetReachabilityState_OFFLINE) ) )
   {
      LOG_DEBUG(logContext, Log_DEBUG, std::string("Secondary is offline and will need resync. ") +
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );

      // buddy is marked offline, so local msg processing will be done and buddy needs resync
      storageTargets->setBuddyNeedsResync(actualTargetID, true);

      return FhgfsOpsErr_SUCCESS; // go ahead with local msg processing
   }

   if(unlikely(commRes != FhgfsOpsErr_SUCCESS) )
   {
      LogContext(logContext).log(Log_DEBUG, "Forwarding failed. "
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) + "; "
         "error: " + FhgfsOpsErrTk::toErrString(commRes) );

      if(*outChunkLocked)
      { // unlock chunk
         chunkLockStore->unlockChunk(actualTargetID, getEntryID() );
         *outChunkLocked = false;
      }

      std::string genericRespStr = "Communication with secondary failed. "
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() );

      ctx.sendResponse(
            GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR, genericRespStr.c_str() ) );

      return FhgfsOpsErr_COMMUNICATION;
   }

   TruncLocalFileRespMsg* respMsg = (TruncLocalFileRespMsg*)rrArgs.outRespMsg;
   FhgfsOpsErr secondaryRes = respMsg->getResult();
   if(unlikely(secondaryRes != FhgfsOpsErr_SUCCESS) )
   {
      if(secondaryRes == FhgfsOpsErr_UNKNOWNTARGET)
      {
         /* local msg processing shall be done and buddy needs resync
            (this is normal when a storage is restarted without a broken secondary target, so we
            report success to a client in this case) */

         LogContext(logContext).log(Log_DEBUG,
            "Secondary reports unknown target error and will need resync. "
            "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );

         storageTargets->setBuddyNeedsResync(actualTargetID, true);

         return FhgfsOpsErr_SUCCESS;
      }

      if(secondaryRes != FhgfsOpsErr_TOOBIG) // "too big" is a valid error if max filesize exceeded
      {
         LogContext(logContext).log(Log_NOTICE, std::string("Secondary reported error: ") +
            FhgfsOpsErrTk::toErrString(secondaryRes) + "; "
            "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );
      }

      return secondaryRes;
   }

   return FhgfsOpsErr_SUCCESS;
}

