#include <toolkit/StorageTkEx.h>
#include <program/Program.h>


#include "StartChunkBalanceMsgEx.h"

FileIDLock StartChunkBalanceMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

bool StartChunkBalanceMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
   const char* logContext = "StartChunkBalanceMsg incoming";
   #endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_SPAM, "Starting ChunkBalance job for chunk in path: "
         + getRelativePath());
   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> StartChunkBalanceMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   StartChunkBalanceMsgResponseState resp;
   FhgfsOpsErr startChunkBalanceMsgRes = FhgfsOpsErr_INTERNAL;
   IdType idType = getIdType();
   const std::string& relativePath = getRelativePath();
   const UInt16Vector& targetIDs = getTargetIDs();
   const UInt16Vector& destinationIDs = getDestinationIDs();
   EntryInfo* entryInfo = getEntryInfo();
   bool isNewJob = false;
   App* app = Program::getApp();
   ChunkBalancerJob* chunkBalanceJob;
   FileEvent* fileEvent = getFileEvent(); // Create and initialize FileEvent object
   if (unlikely(!fileEvent))
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_WARNING, "Invalid FileEvent received, chunk will not be balanced: " + relativePath);
      resp.setResult(startChunkBalanceMsgRes);
      return boost::make_unique<ResponseState>(std::move(resp));
   }

   // try to start a ChunkBalance job on metadata; if it already exists, we get that job
   {
      std::lock_guard<Mutex> mutexLock(app->ChunkBalanceJobMutex);
      chunkBalanceJob = StartChunkBalanceMsgEx::addChunkBalanceJob(isNewJob);
      if (unlikely(!chunkBalanceJob))
      {
         LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_WARNING, "Failed to start ChunkBalanceJob component, chunk will not be balanced: " + relativePath);
         resp.setResult(startChunkBalanceMsgRes);
         return boost::make_unique<ResponseState>(std::move(resp));
      }
      if (!isNewJob && !chunkBalanceJob->isRunningStarting() )
      {
         bool isJoined = false;
         try
         {
            isJoined = chunkBalanceJob->timedjoin(0); // harvest the finished job, returns immediately if still running
         }
         catch (const PThreadException& e)
         {
            LogContext(__func__).logErr(std::string("ChunkBalancerJob join failed: ") + e.what());
            resp.setResult(startChunkBalanceMsgRes);
            return boost::make_unique<ResponseState>(std::move(resp));
         }

         if (!isJoined)
         {
            LogContext(__func__).logErr("Cannot restart ChunkBalancerJob - thread still running despite status indicating otherwise.");
            startChunkBalanceMsgRes = FhgfsOpsErr_AGAIN;
            resp.setResult(startChunkBalanceMsgRes);
            return boost::make_unique<ResponseState>(std::move(resp));
         }

         chunkBalanceJob->resetSelfTerminate();
         chunkBalanceJob->start();
         chunkBalanceJob->setStatus(ChunkBalancerJobState_STARTING);
         LogContext(__func__).log(Log_NOTICE, "Starting new ChunkBalancerJob and accepting work requests.");
      }
   }

   switch (idType)
   {
      case idType_INVALID:  // Invalid idType given
         LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_WARNING, "Invalid idType given, chunk will not be balanced: " + relativePath);
         resp.setResult(startChunkBalanceMsgRes);
         return boost::make_unique<ResponseState>(std::move(resp));
      case idType_TARGET:  // Default use case, nothing to do
         break;
      case idType_GROUP:  // Given IDs are mirror groupIDs, nothing to do at this point
         break;
      case idType_POOL:  // Given IDs are poolIDs, not supported yet
         LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_WARNING, "PoolIDs given but not supported, chunk will not be balanced: " + relativePath);
         resp.setResult(startChunkBalanceMsgRes);
         return boost::make_unique<ResponseState>(std::move(resp));
      default:
         LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_WARNING, "Invalid idType given, chunk will not be balanced: " + relativePath);
         resp.setResult(startChunkBalanceMsgRes);
         return boost::make_unique<ResponseState>(std::move(resp));
   }

   // add information of chunk to be balanced to the queue on primary
   if (!isSecondary)
   {
      if (unlikely(targetIDs.size() != destinationIDs.size()))
      {
         LogContext(__func__).log(Log_NOTICE, "Number of targetIDs and destinationIDs does not match.");
         resp.setResult(startChunkBalanceMsgRes);
         return boost::make_unique<ResponseState>(std::move(resp));
      }

      for (size_t i = 0; i < targetIDs.size(); ++i)
      {
         ChunkSyncCandidateFile candidate(idType, (relativePath).c_str(), targetIDs[i], destinationIDs[i], entryInfo, fileEvent);
         startChunkBalanceMsgRes = chunkBalanceJob->addChunkSyncCandidate(&candidate);
         if (startChunkBalanceMsgRes != FhgfsOpsErr_SUCCESS)
         {
            resp.setResult(startChunkBalanceMsgRes);
            return boost::make_unique<ResponseState>(std::move(resp));
         }
      }
   }

   resp.setResult(startChunkBalanceMsgRes);
   return boost::make_unique<ResponseState>(std::move(resp));
}

ChunkBalancerJob*  StartChunkBalanceMsgEx::addChunkBalanceJob(bool& outIsNew)
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
