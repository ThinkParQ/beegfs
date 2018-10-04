#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsRespMsg.h>
#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include "GetChunkFileAttribsMsgEx.h"


bool GetChunkFileAttribsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetChunkFileAttribsMsg incoming";

   App* app = Program::getApp();

   std::string entryID(getEntryID() );

   FhgfsOpsErr clientErrRes = FhgfsOpsErr_SUCCESS;
   int targetFD;
   struct stat statbuf{};
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

   auto* const target = app->getStorageTargets()->getTarget(targetID);
   if (!target)
   {
      if (isMsgHeaderFeatureFlagSet(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR))
      { /* buddy mirrored file => fail with GenericResp to make the caller retry.
           mgmt will mark this target as (p)offline in a few moments. */
         LOG(GENERAL, NOTICE, "Unknown target ID, refusing request.", targetID);
         ctx.sendResponse(
               GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR, "Unknown target ID"));
         return true;
      }

      LOG(GENERAL, ERR, "Unknown target ID.", targetID);
      clientErrRes = FhgfsOpsErr_UNKNOWNTARGET;
      goto send_response;
   }

   { // get targetFD and check consistency state
      bool skipResponse = false;

      targetFD = getTargetFD(*target, ctx, &skipResponse);
      if(unlikely(targetFD == -1) )
      { // failed => consistency state not good
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
 * @return -1 if consistency state was not good (in which case a special response is sent within
 * this method), otherwise the file descriptor to chunks dir (or mirror dir).
 */
int GetChunkFileAttribsMsgEx::getTargetFD(const StorageTarget& target, ResponseContext& ctx,
      bool* outResponseSent)
{
   bool isBuddyMirrorChunk = isMsgHeaderFeatureFlagSet(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR);

   *outResponseSent = false;

   // get targetFD and check consistency state

   const auto consistencyState = target.getConsistencyState();
   const int targetFD = isBuddyMirrorChunk ? *target.getMirrorFD() : *target.getChunkFD();

   if(unlikely(consistencyState != TargetConsistencyState_GOOD) &&
      isBuddyMirrorChunk &&
      !isMsgHeaderFeatureFlagSet(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR_SECOND) )
   { // this is a msg to a non-good primary
      std::string respMsgLogStr = "Refusing request. Target consistency is not good. "
         "targetID: " + StringTk::uintToStr(target.getID());

      ctx.sendResponse(
            GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR, std::move(respMsgLogStr)));

      *outResponseSent = true;
      return -1;
   }

   return targetFD;
}
