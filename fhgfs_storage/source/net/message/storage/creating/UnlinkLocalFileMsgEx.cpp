#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include "UnlinkLocalFileMsgEx.h"

bool UnlinkLocalFileMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "UnlinkChunkFileMsg incoming";

   #ifdef BEEGFS_DEBUG
      LOG_DEBUG(logContext, Log_DEBUG, "Received a UnlinkLocalFileMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

   App* app = Program::getApp();
   ChunkStore* chunkDirStore = app->getChunkDirStore();

   FhgfsOpsErr clientErrRes = FhgfsOpsErr_SUCCESS;

   uint16_t targetID;
   bool chunkLocked = false;
   int targetFD = -1;
   Path chunkDirPath;
   const PathInfo* pathInfo = getPathInfo();
   bool hasOrigFeature = pathInfo->hasOrigFeature();
   int unlinkRes = -1;

   // select the right targetID

   targetID = getTargetID();

   if(isMsgHeaderFeatureFlagSet(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR) )
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = isMsgHeaderFeatureFlagSet(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) ?
         mirrorBuddies->getSecondaryTargetID(targetID) :
         mirrorBuddies->getPrimaryTargetID(targetID);

      if(unlikely(!targetID) )
      { // unknown target
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

      // generate path to chunk file...

      std::string chunkFilePathStr; // chunkDirPathStr + '/' + entryID

      StorageTk::getChunkDirChunkFilePath(pathInfo, getEntryID(), hasOrigFeature, chunkDirPath,
         chunkFilePathStr);

      unlinkRes = unlinkat(targetFD, chunkFilePathStr.c_str(), 0);

      if( (unlinkRes == -1) && (errno != ENOENT) )
      { // error
         LogContext(logContext).logErr("Unable to unlink file: " + chunkFilePathStr + ". " +
            "SysErr: " + System::getErrString() );

         clientErrRes = FhgfsOpsErr_INTERNAL;
      }
      else
      { // success
         LogContext(logContext).log(Log_DEBUG, "File unlinked: " + chunkFilePathStr);
      }
   }


send_response:

   if(chunkLocked) // unlock chunk
      app->getChunkLockStore()->unlockChunk(targetID, getEntryID() );

   ctx.sendResponse(UnlinkLocalFileRespMsg(clientErrRes) );

skip_response:

   // try to rmdir chunkDirPath (in case this was the last chunkfile in a dir)
   if (!unlinkRes && hasOrigFeature)
      chunkDirStore->rmdirChunkDirPath(targetFD, &chunkDirPath);

   // update operation counters...
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), StorageOpCounter_UNLINK,
      getMsgHeaderUserID() );

   return true;
}

/**
 * @param outResponseSent true if a response was sent from within this method; can only be true if
 * -1 is returned.
 * @return -1 if no such target exists or if consistency state was not good (in which case a special
 * response is sent within this method), otherwise the file descriptor to chunks dir (or mirror
 * dir).
 */
int UnlinkLocalFileMsgEx::getTargetFD(ResponseContext& ctx, uint16_t actualTargetID,
   bool* outResponseSent)
{
   const char* logContext = "UnlinkChunkFileMsg (get target FD)";

   App* app = Program::getApp();

   bool isBuddyMirrorChunk = isMsgHeaderFeatureFlagSet(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR);
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
      !isMsgHeaderFeatureFlagSet(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) )
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

/**
 * If this is a buddy mirror msg and we are the primary, forward this msg to secondary.
 *
 * @return _COMMUNICATION if forwarding to buddy failed and buddy is not marked offline (in which
 *    case *outChunkLocked==false is guaranteed).
 * @throw SocketException if sending of GenericResponseMsg fails.
 */
FhgfsOpsErr UnlinkLocalFileMsgEx::forwardToSecondary(ResponseContext& ctx, uint16_t actualTargetID,
   bool* outChunkLocked)
{
   const char* logContext = "UnlinkLocalFileMsg incoming (forward to secondary)";

   App* app = Program::getApp();
   ChunkLockStore* chunkLockStore = app->getChunkLockStore();
   StorageTargets* storageTargets = app->getStorageTargets();

   *outChunkLocked = false;

   if(!isMsgHeaderFeatureFlagSet(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR) ||
       isMsgHeaderFeatureFlagSet(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) )
      return FhgfsOpsErr_SUCCESS; // nothing to do

   // mirrored chunk should be modified, check if resync is in progress and lock chunk
   *outChunkLocked = storageTargets->isBuddyResyncInProgress(actualTargetID);
   if(*outChunkLocked)
      chunkLockStore->lockChunk(actualTargetID, getEntryID() ); // lock chunk

   // instead of creating a new msg object, we just re-use "this" with "buddymirror second" flag
   addMsgHeaderFeatureFlag(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);

   RequestResponseArgs rrArgs(NULL, this, NETMSGTYPE_UnlinkLocalFileResp);
   RequestResponseTarget rrTarget(getTargetID(), app->getTargetMapper(), app->getStorageNodes(),
      app->getTargetStateStore(), app->getMirrorBuddyGroupMapper(), true);

   FhgfsOpsErr commRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

   // remove the flag that we just added for secondary
   unsetMsgHeaderFeatureFlag(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);

   if(unlikely(
      (commRes == FhgfsOpsErr_COMMUNICATION) &&
      (rrTarget.outTargetReachabilityState == TargetReachabilityState_OFFLINE) ) )
   {
      LOG_DEBUG(logContext, Log_DEBUG, std::string("Secondary is offline and will need resync. ") +
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );

      // buddy is marked offline, so local msg processing will be done and buddy needs resync
      auto secTargetID = app->getMirrorBuddyGroupMapper()->getSecondaryTargetID(getTargetID());
      storageTargets->setBuddyNeedsResync(actualTargetID, true, secTargetID);

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

   UnlinkLocalFileRespMsg* respMsg = (UnlinkLocalFileRespMsg*)rrArgs.outRespMsg;
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

         auto secTargetID = app->getMirrorBuddyGroupMapper()->getSecondaryTargetID(getTargetID());
         storageTargets->setBuddyNeedsResync(actualTargetID, true, secTargetID);

         return FhgfsOpsErr_SUCCESS;
      }

      LogContext(logContext).log(Log_NOTICE, std::string("Secondary reported error: ") +
         FhgfsOpsErrTk::toErrString(secondaryRes) + "; "
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );

      return secondaryRes;
   }


   return FhgfsOpsErr_SUCCESS;
}
