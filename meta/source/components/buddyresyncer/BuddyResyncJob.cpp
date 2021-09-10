#include <program/Program.h>

#include <common/components/worker/IncSyncedCounterWork.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/net/message/storage/mirroring/StorageResyncStartedMsg.h>
#include <common/net/message/storage/mirroring/StorageResyncStartedRespMsg.h>
#include <common/threading/Barrier.h>
#include <common/toolkit/DebugVariable.h>
#include <common/toolkit/SynchronizedCounter.h>

#include <app/App.h>
#include <components/buddyresyncer/BuddyResyncerBulkSyncSlave.h>
#include <components/buddyresyncer/BuddyResyncerModSyncSlave.h>
#include <components/worker/BarrierWork.h>
#include <toolkit/BuddyCommTk.h>

#include "BuddyResyncJob.h"

BuddyResyncJob::BuddyResyncJob() :
   PThread("BuddyResyncJob"),
   state(BuddyResyncJobState_NOTSTARTED),
   startTime(0), endTime(0),
   gatherSlave(boost::make_unique<BuddyResyncerGatherSlave>(&syncCandidates))
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   buddyNodeID =
      NumNodeID(app->getMetaBuddyGroupMapper()->getBuddyTargetID(app->getLocalNodeNumID().val()));

   const unsigned numSyncSlaves = std::max<unsigned>(cfg->getTuneNumResyncSlaves(), 1);

   for (size_t i = 0; i < numSyncSlaves; i++)
      bulkSyncSlaves.emplace_back(
            boost::make_unique<BuddyResyncerBulkSyncSlave>(*this, &syncCandidates, i, buddyNodeID));

   sessionStoreResyncer = boost::make_unique<SessionStoreResyncer>(buddyNodeID);
   modSyncSlave = boost::make_unique<BuddyResyncerModSyncSlave>(*this, &syncCandidates, 1, buddyNodeID);
}

BuddyResyncJob::~BuddyResyncJob() = default;

void BuddyResyncJob::run()
{
   const char* logContext = "Run resync job";

   InternodeSyncer* internodeSyncer = Program::getApp()->getInternodeSyncer();
   App* app = Program::getApp();
   WorkerList* workers = app->getWorkers();
   NodeStore* metaNodes = app->getMetaNodes();
   const std::string metaPath = app->getMetaPath();
   const std::string metaBuddyMirPath = app->getMetaPath() + "/" + CONFIG_BUDDYMIRROR_SUBDIR_NAME;
   Barrier workerBarrier(workers->size() + 1);
   bool workersStopped = false;

   startTime = time(NULL);

   syncCandidates.clear();

   auto buddyNode = metaNodes->referenceNode(buddyNodeID);

   if (!buddyNode)
   {
      LOG(MIRRORING, ERR, "Unable to resolve buddy node. Resync will not start.");
      setState(BuddyResyncJobState_FAILURE);
      goto cleanup;
   }

   DEBUG_ENV_VAR(unsigned, DIE_AT_RESYNC_N, 0, "BEEGFS_RESYNC_DIE_AT_N");
   if (DIE_AT_RESYNC_N) {
      static unsigned resyncs = 0;
      // for #479: terminating a server at this point caused the workers to terminate before the
      // resyncer had communicated with them, causing a deadlock on shutdown
      if (++resyncs == DIE_AT_RESYNC_N) {
         ::kill(0, SIGTERM);
         sleep(4);
      }
   }
   stopAllWorkersOn(workerBarrier);
   {
      // Notify buddy that resync started and wait for confirmation
      StorageResyncStartedMsg msg(buddyNodeID.val());
      const auto respMsg = MessagingTk::requestResponse(*buddyNode, msg,
            NETMSGTYPE_StorageResyncStartedResp);

      if (!respMsg)
      {
         LogContext(logContext).logErr("Unable to notify buddy about resync attempt. "
               "Resync will not start.");
         setState(BuddyResyncJobState_FAILURE);
         workerBarrier.wait();
         goto cleanup;
      }

      // resync could have been aborted before we got here. if so, exit as soon as possible without
      // setting the resync job state to something else.
      {
         std::unique_lock<Mutex> lock(stateMutex);

         if (state == BuddyResyncJobState_INTERRUPTED)
         {
            lock.unlock();
            workerBarrier.wait();
            goto cleanup;
         }

         state = BuddyResyncJobState_RUNNING;
      }
      internodeSyncer->setResyncInProgress(true);

      const bool startGatherSlaveRes = startGatherSlaves();
      if (!startGatherSlaveRes)
      {
         setState(BuddyResyncJobState_FAILURE);
         workerBarrier.wait();
         goto cleanup;
      }

      const bool startResyncSlaveRes = startSyncSlaves();
      if (!startResyncSlaveRes)
      {
         setState(BuddyResyncJobState_FAILURE);
         workerBarrier.wait();
         goto cleanup;
      }
   }
   workerBarrier.wait();

   LOG_DEBUG(__func__, Log_DEBUG, "Going to join gather slaves.");
   joinGatherSlaves();
   LOG_DEBUG(__func__, Log_DEBUG, "Joined gather slaves.");

   LOG_DEBUG(__func__, Log_DEBUG, "Going to join sync slaves.");

   // gather slaves have finished. Tell sync slaves to stop when work packages are empty and wait.
   for (auto it = bulkSyncSlaves.begin(); it != bulkSyncSlaves.end(); ++it)
   {
      (*it)->setOnlyTerminateIfIdle(true);
      (*it)->selfTerminate();
   }

   for (auto it = bulkSyncSlaves.begin(); it != bulkSyncSlaves.end(); ++it)
      (*it)->join();

   // here we can be in one of two situations:
   //  1. bulk resync has succeeded. we then totally stop the workers: the session store must be in
   //     a quiescent state for resync, so for simplicitly, we suspend all client operations here.
   //     we do not want to do this any earlier than this point, because bulk syncers may take a
   //     very long time to complete.
   //  2. bulk resync has failed. in this case, the bulk syncers have aborted the currently running
   //     job, and the mod syncer is either dead or in the process of dying. here we MUST NOT stop
   //     the workers, because they are very likely blocked on the mod sync queue already and will
   //     not unblock before the queue is cleared.
   if (getState() == BuddyResyncJobState_RUNNING)
   {
      stopAllWorkersOn(workerBarrier);
      workersStopped = true;
   }

   modSyncSlave->setOnlyTerminateIfIdle(true);
   modSyncSlave->selfTerminate();
   modSyncSlave->join();

   // gatherers are done and the workers have been stopped, we can safely resync the session now.

   LOG_DEBUG(__func__, Log_DEBUG, "Joined sync slaves.");

   // Perform session store resync
   // the job may have been aborted or terminated by errors. in this case, do not resync the session
   // store. end the sync as quickly as possible.
   if (getState() == BuddyResyncJobState_RUNNING)
      sessionStoreResyncer->doSync();

   // session store is now synced, and future actions can be forwarded safely. we do not restart
   // the workers here because the resync may still enter FAILED state, and we don't want to forward
   // to the secondary in this case.

cleanup:
   bool syncErrors = false;

   {
      std::lock_guard<Mutex> lock(gatherSlave->stateMutex);
      while (gatherSlave->isRunning)
         gatherSlave->isRunningChangeCond.wait(&gatherSlave->stateMutex);

      syncErrors |= gatherSlave->getStats().errors != 0;
   }

   for (auto it = bulkSyncSlaves.begin(); it != bulkSyncSlaves.end(); ++it)
   {
      BuddyResyncerBulkSyncSlave* slave = it->get();
      std::lock_guard<Mutex> lock(slave->stateMutex);
      while (slave->isRunning)
         slave->isRunningChangeCond.wait(&slave->stateMutex);

      syncErrors |= slave->getStats().dirErrors != 0;
      syncErrors |= slave->getStats().fileErrors != 0;
   }

   syncErrors |= sessionStoreResyncer->getStats().errors;

   {
      while (modSyncSlave->isRunning)
         modSyncSlave->isRunningChangeCond.wait(&modSyncSlave->stateMutex);

      syncErrors |= modSyncSlave->getStats().errors != 0;
   }


   if (getState() == BuddyResyncJobState_RUNNING || getState() == BuddyResyncJobState_INTERRUPTED)
   {
      if (syncErrors)
         setState(BuddyResyncJobState_ERRORS);
      else if (getState() == BuddyResyncJobState_RUNNING)
         setState(BuddyResyncJobState_SUCCESS);

      // delete timestamp override file if it exists.
      BuddyCommTk::setBuddyNeedsResync(metaPath, false);

      const TargetConsistencyState buddyState = newBuddyState();
      informBuddy(buddyState);
      informMgmtd(buddyState);

      const bool interrupted = getState() != BuddyResyncJobState_SUCCESS;
      LOG(MIRRORING, WARNING, "Resync finished.", interrupted, syncErrors);
   }

   internodeSyncer->setResyncInProgress(false);
   endTime = time(NULL);

   // restart all the worker threads
   if (workersStopped)
      workerBarrier.wait();

   // if the resync was aborted, the mod sync queue may still contain items. additionally, workers
   // may be waiting for a changeset slot, or they may have started executing after the resync was
   // aborted by the sync slaves, but before the resync was officially set to "not running".
   // we cannot set the resync to "not running" in abort() because we have no upper bound for the
   // number of worker threads. even if we did set the resync to "not running" in abort() and
   // cleared the sync queues at the same time, there may still be an arbitrary number of threads
   // waiting for a changeset slot.
   // instead, we have to wait for each thread to "see" that the resync is over, and periodically
   // clear the sync queue to unblock those workers that are still waiting for slots.
   if (syncErrors)
   {
      SynchronizedCounter counter;

      for (auto it = workers->begin(); it != workers->end(); ++it)
      {
         auto& worker = **it;

         worker.getWorkQueue()->addPersonalWork(
               new IncSyncedCounterWork(&counter),
               worker.getPersonalWorkQueue());
      }

      while (!counter.timedWaitForCount(workers->size(), 100))
      {
         while (!syncCandidates.isFilesEmpty())
         {
            MetaSyncCandidateFile candidate;
            syncCandidates.fetch(candidate, this);
            candidate.signal();
         }
      }
   }
}

void BuddyResyncJob::stopAllWorkersOn(Barrier& barrier)
{
   WorkerList* workers = Program::getApp()->getWorkers();

   for (WorkerListIter workerIt = workers->begin(); workerIt != workers->end(); ++workerIt)
   {
      Worker* worker = *workerIt;
      PersonalWorkQueue* personalQ = worker->getPersonalWorkQueue();
      MultiWorkQueue* workQueue = worker->getWorkQueue();
      workQueue->addPersonalWork(new BarrierWork(&barrier), personalQ);
   }

   barrier.wait(); // Wait until all workers are blocked.
}

void BuddyResyncJob::abort(bool wait_for_completion)
{
   setState(BuddyResyncJobState_INTERRUPTED);

   gatherSlave->selfTerminate();

   // set onlyTerminateIfIdle on the slaves to false - they will be stopped by the main loop then.
   for (auto it = bulkSyncSlaves.begin(); it != bulkSyncSlaves.end(); ++it)
   {
      BuddyResyncerBulkSyncSlave* slave = it->get();
      slave->setOnlyTerminateIfIdle(false);
   }

   modSyncSlave->selfTerminate();

   int retry = 600;
   /* Wait till all on-going thread events are fetched or max 30mins.
    * (fetch waits for 3secs if there are no files to be fetched)
    */
   if (wait_for_completion)
   {
      modSyncSlave->join();
      while (threadCount > 0 && retry)
      {
         LOG(MIRRORING, WARNING, "Wait for pending worker threads to finish");
         if (!syncCandidates.isFilesEmpty())
         {
            MetaSyncCandidateFile candidate;
            syncCandidates.fetch(candidate, this);
            candidate.signal();
         }
         retry--;
      }
      if (threadCount)
         LOG(MIRRORING, ERR, "Cleanup of aborted resync failed: I/O worker threads"
                           " did not finish properly: ",
                           ("threadCount", threadCount.load()));
   }
}

bool BuddyResyncJob::startGatherSlaves()
{
   try
   {
      gatherSlave->resetSelfTerminate();
      gatherSlave->start();
      gatherSlave->setIsRunning(true);
   }
   catch (PThreadCreateException& e)
   {
      LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what());
      return false;
   }

   return true;
}

bool BuddyResyncJob::startSyncSlaves()
{
   App* app = Program::getApp();
   const NumNodeID localNodeID = app->getLocalNodeNumID();
   const NumNodeID buddyNodeID(
      app->getMetaBuddyGroupMapper()->getBuddyTargetID(localNodeID.val(), NULL) );

   for (size_t i = 0; i < bulkSyncSlaves.size(); i++)
   {
      try
      {
         bulkSyncSlaves[i]->resetSelfTerminate();
         bulkSyncSlaves[i]->start();
         bulkSyncSlaves[i]->setIsRunning(true);
      }
      catch (PThreadCreateException& e)
      {
         LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what() );

         for (size_t j = 0; j < i; j++)
            bulkSyncSlaves[j]->selfTerminate();

         return false;
      }
   }

   try
   {
      modSyncSlave->resetSelfTerminate();
      modSyncSlave->start();
      modSyncSlave->setIsRunning(true);
   }
   catch (PThreadCreateException& e)
   {
      LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what() );

      for (size_t j = 0; j < bulkSyncSlaves.size(); j++)
         bulkSyncSlaves[j]->selfTerminate();

      return false;
   }

   return true;
}

void BuddyResyncJob::joinGatherSlaves()
{
   gatherSlave->join();
}

MetaBuddyResyncJobStatistics BuddyResyncJob::getJobStats()
{
   std::lock_guard<Mutex> lock(stateMutex);

   BuddyResyncerGatherSlave::Stats gatherStats = gatherSlave->getStats();
   const uint64_t dirsDiscovered = gatherStats.dirsDiscovered;
   const uint64_t gatherErrors = gatherStats.errors;

   uint64_t dirsSynced = 0;
   uint64_t filesSynced = 0;
   uint64_t dirErrors = 0;
   uint64_t fileErrors = 0;

   for (auto syncerIt = bulkSyncSlaves.begin(); syncerIt != bulkSyncSlaves.end(); ++syncerIt)
   {
      BuddyResyncerBulkSyncSlave::Stats bulkSyncStats = (*syncerIt)->getStats();
      dirsSynced += bulkSyncStats.dirsSynced;
      filesSynced += bulkSyncStats.filesSynced;
      dirErrors += bulkSyncStats.dirErrors;
      fileErrors += bulkSyncStats.fileErrors;
   }

   SessionStoreResyncer::Stats sessionSyncStats = sessionStoreResyncer->getStats();
   const uint64_t sessionsToSync = sessionSyncStats.sessionsToSync;
   const uint64_t sessionsSynced = sessionSyncStats.sessionsSynced;
   const bool sessionSyncErrors = sessionSyncStats.errors;

   BuddyResyncerModSyncSlave::Stats modSyncStats = modSyncSlave->getStats();
   uint64_t modObjectsSynced = modSyncStats.objectsSynced;
   uint64_t modSyncErrors = modSyncStats.errors;

   return MetaBuddyResyncJobStatistics(
         state, startTime, endTime,
         dirsDiscovered, gatherErrors,
         dirsSynced, filesSynced, dirErrors, fileErrors,
         sessionsToSync, sessionsSynced, sessionSyncErrors,
         modObjectsSynced, modSyncErrors);
}

/**
 * Determine the state for the buddy after the end of a resync job.
 * @returns the new state to be set on the buddy accroding to this job's JobState.
 */
TargetConsistencyState BuddyResyncJob::newBuddyState()
{
   switch (getState())
   {
      case BuddyResyncJobState_ERRORS:
      case BuddyResyncJobState_INTERRUPTED:
      case BuddyResyncJobState_FAILURE:
         return TargetConsistencyState_BAD;

      case BuddyResyncJobState_SUCCESS:
         return TargetConsistencyState_GOOD;

      default:
         LOG(MIRRORING, ERR, "Undefined resync state.", state);
         return TargetConsistencyState_BAD;
   }
}

void BuddyResyncJob::informBuddy(const TargetConsistencyState newTargetState)
{
   App* app = Program::getApp();
   NodeStore* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* buddyGroups = app->getMetaBuddyGroupMapper();

   NumNodeID buddyNodeID =
      NumNodeID(buddyGroups->getBuddyTargetID(app->getLocalNodeNumID().val()));
   auto metaNode = metaNodes->referenceNode(buddyNodeID);

   if (!metaNode)
   {
      LOG(MIRRORING, ERR, "Unable to inform buddy about finished resync", buddyNodeID.str());
      return;
   }

   UInt16List nodeIDs(1, buddyNodeID.val());
   UInt8List states(1, newTargetState);
   SetTargetConsistencyStatesMsg msg(NODETYPE_Meta, &nodeIDs, &states, false);

   const auto respMsg = MessagingTk::requestResponse(*metaNode, msg,
         NETMSGTYPE_SetTargetConsistencyStatesResp);
   if (!respMsg)
   {
      LogContext(__func__).logErr(
         "Unable to inform buddy about finished resync. "
         "BuddyNodeID: " + buddyNodeID.str() + "; "
         "error: Communication Error");
      return;
   }

   {
      auto* respMsgCast = static_cast<SetTargetConsistencyStatesRespMsg*>(respMsg.get());
      FhgfsOpsErr result = respMsgCast->getResult();

      if (result != FhgfsOpsErr_SUCCESS)
      {
         LogContext(__func__).logErr(
            "Error while informing buddy about finished resync. "
            "BuddyNodeID: " + buddyNodeID.str() + "; "
            "error: " + boost::lexical_cast<std::string>(result) );
      }
   }
}

void BuddyResyncJob::informMgmtd(const TargetConsistencyState newTargetState)
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   if (!mgmtNode)
   {
      LOG(MIRRORING, ERR, "Unable to communicate with management node.");
      return;
   }

   UInt16List nodeIDs(1, buddyNodeID.val());
   UInt8List states(1, newTargetState);
   SetTargetConsistencyStatesMsg msg(NODETYPE_Meta, &nodeIDs, &states, false);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg,
         NETMSGTYPE_SetTargetConsistencyStatesResp);
   if (!respMsg)
   {
      LOG(MIRRORING, ERR,
            "Unable to inform management node about finished resync: Communication error.");
      return;
   }

   {
      auto* respMsgCast = static_cast<SetTargetConsistencyStatesRespMsg*>(respMsg.get());
      FhgfsOpsErr result = respMsgCast->getResult();

      if (result != FhgfsOpsErr_SUCCESS)
         LOG(MIRRORING, ERR, "Error informing management node about finished resync.", result);
   }
}
