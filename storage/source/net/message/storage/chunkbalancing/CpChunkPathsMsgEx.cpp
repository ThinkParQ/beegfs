#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/chunkbalancing/CpChunkPathsRespMsg.h>
#include <toolkit/StorageTkEx.h>
#include <program/Program.h>
#include <components/chunkbalancer/ChunkBalancerFileSyncSlave.h>

#include "CpChunkPathsMsgEx.h"

bool CpChunkPathsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "CpChunkPathsMsg incoming";

   uint16_t targetID = getTargetID();
   uint16_t destinationID = getDestinationID();
   std::string& relativePath = getRelativePath();
   EntryInfo* entryInfo = getEntryInfo();
   FileEvent* fileEvent = getFileEvent();
   StorageTarget* target;
   FhgfsOpsErr cpMsgRes = FhgfsOpsErr_INTERNAL;;
   bool isNewJob = false;

   App* app = Program::getApp();
   bool isBuddyMirrorChunk = isMsgHeaderFeatureFlagSet(CPCHUNKPATHSMSG_FLAG_BUDDYMIRROR);

   if (isBuddyMirrorChunk)
   { // given source targetID refers to a buddy mirror group ID, turn it into primary targetID
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = mirrorBuddies->getPrimaryTargetID(targetID);
      if (unlikely(!targetID) )
      { // unknown target
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            std::to_string(getTargetID() ) );
         cpMsgRes = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }

      // given destinationID targetID refers to a buddy mirror group ID, turn it into primary targetID
      destinationID = mirrorBuddies->getPrimaryTargetID(destinationID);
      if (unlikely(!destinationID) )
      { // unknown target
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            std::to_string(getDestinationID() ) );
         cpMsgRes = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }
   }
   target = app->getStorageTargets()->getTarget(targetID);

   if (!target)
   { // unknown targetID
      if (isBuddyMirrorChunk)
      { /* buddy mirrored file => fail with GenericResp to make the caller retry.
           mgmt will mark this target as (p)offline in a few moments. */
         ctx.sendResponse(
               GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR, "Unknown target ID"));
         return true;
      }

      LogContext(logContext).logErr("Unknown targetID: " + std::to_string(targetID));
      cpMsgRes = FhgfsOpsErr_UNKNOWNTARGET;
      goto send_response;
   }

   { // valid targetID
      std::lock_guard<Mutex> mutexLock(app->ChunkBalanceJobMutex);
      // Try to start a ChunkBalance job; if it already exists, we get that job
      ChunkBalancerJob* chunkBalanceJob = CpChunkPathsMsgEx::addChunkBalanceJob(isNewJob);
      if (unlikely(!chunkBalanceJob))
      {
         LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_WARNING, "Failed to start ChunkBalancerJob component, chunk will not be balanced: " + relativePath);
         goto send_response;
      }
      // Start thread if job already exists but is not running
      if (!isNewJob && !chunkBalanceJob->isRunningStarting())
      {
         bool isJoined = false;
         try
         {
            isJoined = chunkBalanceJob->timedjoin(0); // harvest the finished job, returns immediately if still running
         }
         catch (const PThreadException& e)
         {
            LogContext(__func__).logErr(std::string("ChunkBalancerJob join failed: ") + e.what());
            goto send_response;
         }

         if (!isJoined)
         {
            LogContext(__func__).logErr("Cannot restart ChunkBalancerJob - thread still running despite status indicating otherwise.");
            cpMsgRes = FhgfsOpsErr_AGAIN;
            goto send_response;
         }

         chunkBalanceJob->resetSelfTerminate();
         chunkBalanceJob->start();
         chunkBalanceJob->setStatus(ChunkBalancerJobState_STARTING);
         LogContext(__func__).log(Log_NOTICE, "Starting new ChunkBalancerJob and accepting work requests.");
      }
      ChunkSyncCandidateFile candidate((relativePath).c_str(), targetID, destinationID, entryInfo, isBuddyMirrorChunk, fileEvent);
      cpMsgRes = chunkBalanceJob->addChunkSyncCandidate(&candidate);
   }

send_response:
   ctx.sendResponse(CpChunkPathsRespMsg(cpMsgRes));
   return true;
}

ChunkBalancerJob* CpChunkPathsMsgEx::addChunkBalanceJob(bool& outIsNew)
{
   App* app = Program::getApp();
   ChunkBalancerJob* chunkBalanceJob = app->getChunkBalancerJob();
   if (chunkBalanceJob == nullptr)
   {
      chunkBalanceJob = new ChunkBalancerJob();
      chunkBalanceJob->start();
      chunkBalanceJob->setStatus(ChunkBalancerJobState_STARTING);
      LogContext(__func__).log(Log_NOTICE, "Starting new ChunkBalancerJob and accepting work requests.");
      app->setChunkBalancerJob(chunkBalanceJob);
      outIsNew = true;
   }
   return chunkBalanceJob;
}
