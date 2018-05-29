#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsRespMsg.h>
#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include "GetChunkFileAttribsMsgEx.h"


bool GetChunkFileAttribsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetChunkFileAttribsMsg incoming";

   #ifdef BEEGFS_DEBUG
      LOG_DEBUG(logContext, Log_DEBUG, "Received a GetChunkFileAttribsMsg from: " + ctx.peerName());
   #endif // BEEGFS_DEBUG
   
   App* app = Program::getApp();

   std::string entryID(getEntryID() );

   FhgfsOpsErr clientErrRes = FhgfsOpsErr_SUCCESS;
   int targetFD;
   struct stat statbuf;
   uint64_t storageVersion = 0;

   // select the right targetID

   uint16_t targetID = getTargetID();

   if(isMsgHeaderFeatureFlagSet(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR) )
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = isMsgHeaderFeatureFlagSet(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR_SECOND) ?
         mirrorBuddies->getSecondaryTargetID(targetID) :
         mirrorBuddies->getPrimaryTargetID(targetID);

      // note: only log message here, error handling will happen below through invalid targetFD
      if(unlikely(!targetID) )
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            StringTk::uintToStr(getTargetID() ) );
   }

   { // get targetFD and check consistency state
      bool skipResponse = false;

      targetFD = getTargetFD(ctx, targetID, &skipResponse);
      if(unlikely(targetFD == -1) )
      { // failed => either unknown targetID or consistency state not good
         memset(&statbuf, 0, sizeof(statbuf) ); // (just to mute clang warning)

         if(skipResponse)
            goto skip_response; // GenericResponseMsg sent

         clientErrRes = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }
   }

   { // valid targetID
      SyncedStoragePaths* syncedPaths = app->getSyncedStoragePaths();

      int statErrCode = 0;

      std::string chunkPath = StorageTk::getFileChunkPath(getPathInfo(), entryID);

      uint64_t newStorageVersion = syncedPaths->lockPath(entryID, targetID); // L O C K path

      int statRes = fstatat(targetFD, chunkPath.c_str(), &statbuf, 0);
      if(statRes)
      { // file not exists or error
         statErrCode = errno;
      }
      else
      {
         storageVersion = newStorageVersion;
      }

      syncedPaths->unlockPath(entryID, targetID); // U N L O C K path

      // note: non-existing file is not an error (storage version is 0, so nothing will be
      //    updated at the metadata node)

      if((statRes == -1) && (statErrCode != ENOENT))
      { // error
         clientErrRes = FhgfsOpsErr_INTERNAL;

         LogContext(logContext).logErr(
            "Unable to stat file: " + chunkPath + ". " + "SysErr: "
               + System::getErrString(statErrCode));
      }
   }

send_response:
   ctx.sendResponse(
         GetChunkFileAttribsRespMsg(clientErrRes, statbuf.st_size, statbuf.st_blocks,
            statbuf.st_mtime, statbuf.st_atime, storageVersion) );

skip_response:

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      StorageOpCounter_GETLOCALFILESIZE, getMsgHeaderUserID() );

   return true;
}

/**
 * @param outResponseSent true if a response was sent from within this method; can only be true if
 * -1 is returned.
 * @return -1 if no such target exists or if consistency state was not good (in which case a special
 * response is sent within this method), otherwise the file descriptor to chunks dir (or mirror
 * dir).
 */
int GetChunkFileAttribsMsgEx::getTargetFD(ResponseContext& ctx, uint16_t actualTargetID,
   bool* outResponseSent)
{
   const char* logContext = "TruncChunkFileMsg (get target FD)";

   App* app = Program::getApp();

   bool isBuddyMirrorChunk = isMsgHeaderFeatureFlagSet(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR);
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
      !isMsgHeaderFeatureFlagSet(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR_SECOND) )
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
