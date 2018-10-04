#include <program/Program.h>

#include <common/components/worker/IncSyncedCounterWork.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/net/message/storage/mirroring/StorageResyncStartedMsg.h>
#include <common/net/message/storage/mirroring/StorageResyncStartedRespMsg.h>
#include <common/toolkit/StringTk.h>
#include "BuddyResyncJob.h"

#include <boost/lexical_cast.hpp>

#define BUDDYRESYNCJOB_MAXDIRWALKDEPTH 2

BuddyResyncJob::BuddyResyncJob(uint16_t targetID) :
   PThread("BuddyResyncJob_" + StringTk::uintToStr(targetID)),
   targetID(targetID),
   status(BuddyResyncJobState_NOTSTARTED),
   startTime(0), endTime(0)
{
   App* app = Program::getApp();
   unsigned numGatherSlaves = app->getConfig()->getTuneNumResyncGatherSlaves();
   unsigned numSyncSlavesTotal = app->getConfig()->getTuneNumResyncSlaves();
   unsigned numFileSyncSlaves = BEEGFS_MAX((numSyncSlavesTotal / 2), 1);
   unsigned numDirSyncSlaves = BEEGFS_MAX((numSyncSlavesTotal / 2), 1);

   // prepare slaves (vectors) and result vector
   gatherSlaveVec.resize(numGatherSlaves);
   fileSyncSlaveVec.resize(numFileSyncSlaves);
   dirSyncSlaveVec.resize(numDirSyncSlaves);
}

BuddyResyncJob::~BuddyResyncJob()
{
   for(BuddyResyncerGatherSlaveVecIter iter = gatherSlaveVec.begin(); iter != gatherSlaveVec.end();
      iter++)
   {
      BuddyResyncerGatherSlave* slave = *iter;
      SAFE_DELETE(slave);
   }

   for(BuddyResyncerFileSyncSlaveVecIter iter = fileSyncSlaveVec.begin();
      iter != fileSyncSlaveVec.end(); iter++)
   {
      BuddyResyncerFileSyncSlave* slave = *iter;
      SAFE_DELETE(slave);
   }

   for(BuddyResyncerDirSyncSlaveVecIter iter = dirSyncSlaveVec.begin();
      iter != dirSyncSlaveVec.end(); iter++)
   {
      BuddyResyncerDirSyncSlave* slave = *iter;
      SAFE_DELETE(slave);
   }
}

void BuddyResyncJob::run()
{
   // make sure only one job at a time can run!
   {
      std::lock_guard<Mutex> mutexLock(statusMutex);

      if (status == BuddyResyncJobState_RUNNING)
      {
         LogContext(__func__).logErr("Refusing to run same BuddyResyncJob twice!");
         return;
      }
      else
      {
         status = BuddyResyncJobState_RUNNING;
         startTime = time(NULL);
         endTime = 0;
      }
   }

   App* app = Program::getApp();
   StorageTargets* storageTargets = app->getStorageTargets();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   WorkerList* workerList = app->getWorkers();

   bool startGatherSlavesRes;
   bool startSyncSlavesRes;

   std::string targetPath;
   std::string chunksPath;

   bool buddyCommIsOverride = false; // treat errors during lastbuddycomm read as "0, no override"
   int64_t lastBuddyCommTimeSecs;
   int64_t lastBuddyCommSafetyThresholdSecs;
   bool checkTopLevelDirRes;
   bool walkRes;

   auto& target = *storageTargets->getTargets().at(targetID);

   shallAbort.setZero();
   targetWasOffline = false;

   // delete sync candidates and gather queue; just in case there was something from a previous run
   syncCandidates.clear();
   gatherSlavesWorkQueue.clear();

   target.setBuddyResyncInProgress(true);

   LogContext(__func__).log(Log_NOTICE,
      "Started resync of targetID " + StringTk::uintToStr(targetID));

   // before starting the threads make sure every worker knows about the resync (the current work
   // package must be finished), for that we use a dummy package
   Mutex mutex;
   Condition counterIncrementedCond;

   SynchronizedCounter numReadyWorkers;
   size_t numWorkers = workerList->size();
   for (WorkerListIter iter = workerList->begin(); iter != workerList->end(); iter++)
   {
      Worker* worker = *iter;
      PersonalWorkQueue* personalQueue = worker->getPersonalWorkQueue();
      MultiWorkQueue* workQueue = worker->getWorkQueue();
      IncSyncedCounterWork* incCounterWork = new IncSyncedCounterWork(&numReadyWorkers);

      workQueue->addPersonalWork(incCounterWork, personalQueue);
   }

   numReadyWorkers.waitForCount(numWorkers);

   // notify buddy, that resync started and wait for confirmation
   uint16_t buddyTargetID = buddyGroupMapper->getBuddyTargetID(targetID);
   NumNodeID buddyNodeID = targetMapper->getNodeID(buddyTargetID);
   auto buddyNode = storageNodes->referenceNode(buddyNodeID);
   StorageResyncStartedMsg storageResyncStartedMsg(buddyTargetID);
   const auto respMsg = MessagingTk::requestResponse(*buddyNode, storageResyncStartedMsg,
         NETMSGTYPE_StorageResyncStartedResp);

   std::pair<bool, std::chrono::system_clock::time_point> lastBuddyComm;

   if (!respMsg)
   {
      LOG(MIRRORING, ERR, "Unable to notify buddy about resync attempt. Resync will not start.",
            targetID, buddyTargetID);
      setStatus(BuddyResyncJobState_FAILURE);
      goto cleanup;
   }

   startGatherSlavesRes = startGatherSlaves(target);
   if (!startGatherSlavesRes)
   {
      setStatus(BuddyResyncJobState_FAILURE);
      goto cleanup;
   }

   startSyncSlavesRes = startSyncSlaves();
   if (!startSyncSlavesRes)
   {
      setStatus(BuddyResyncJobState_FAILURE);

      // terminate gather slaves
      for (size_t i = 0; i < gatherSlaveVec.size(); i++)
         gatherSlaveVec[i]->selfTerminate();

      goto cleanup;
   }

   numDirsDiscovered.setZero();
   numDirsMatched.setZero();

   // walk over the directories until we reach a certain level and then pass the direcories to
   // gather slaves to parallelize it
   targetPath = target.getPath().str();
   chunksPath = targetPath + "/" + CONFIG_BUDDYMIRROR_SUBDIR_NAME;

   lastBuddyComm = target.getLastBuddyComm();
   buddyCommIsOverride = lastBuddyComm.first;
   lastBuddyCommTimeSecs = std::chrono::system_clock::to_time_t(lastBuddyComm.second);

   lastBuddyCommSafetyThresholdSecs = app->getConfig()->getSysResyncSafetyThresholdMins()*60;
   if ( (lastBuddyCommSafetyThresholdSecs == 0) && (!buddyCommIsOverride) ) // ignore timestamp file
      lastBuddyCommTimeSecs = 0;
   else
   if (lastBuddyCommTimeSecs > lastBuddyCommSafetyThresholdSecs)
      lastBuddyCommTimeSecs -= lastBuddyCommSafetyThresholdSecs;

   checkTopLevelDirRes = checkTopLevelDir(chunksPath, lastBuddyCommTimeSecs);
   if (!checkTopLevelDirRes)
   {
      setStatus(BuddyResyncJobState_FAILURE);

      // terminate gather slaves
      for (size_t i = 0; i < gatherSlaveVec.size(); i++)
         gatherSlaveVec[i]->selfTerminate();

      // terminate sync slaves
      for (size_t i = 0; i < fileSyncSlaveVec.size(); i++)
         fileSyncSlaveVec[i]->selfTerminate();

      for (size_t i = 0; i < dirSyncSlaveVec.size(); i++)
         dirSyncSlaveVec[i]->selfTerminate();

      goto cleanup;
   }

   walkRes = walkDirs(chunksPath, "", 0, lastBuddyCommTimeSecs);
   if (!walkRes)
   {
      setStatus(BuddyResyncJobState_FAILURE);

      // terminate gather slaves
      for (size_t i = 0; i < gatherSlaveVec.size(); i++)
         gatherSlaveVec[i]->selfTerminate();

      // terminate sync slaves
      for (size_t i = 0; i < fileSyncSlaveVec.size(); i++)
         fileSyncSlaveVec[i]->selfTerminate();

      for (size_t i = 0; i < dirSyncSlaveVec.size(); i++)
         dirSyncSlaveVec[i]->selfTerminate();

      goto cleanup;
   }

   // all directories are read => tell gather slave to stop when work queue is empty and wait for
   // all to stop
   for(size_t i = 0; i < gatherSlaveVec.size(); i++)
   {
      if (likely(shallAbort.read() == 0))
         gatherSlaveVec[i]->setOnlyTerminateIfIdle(true);
      else
         gatherSlaveVec[i]->setOnlyTerminateIfIdle(false);

      gatherSlaveVec[i]->selfTerminate();
   }

   joinGatherSlaves();

   // gather slaves have finished => tell sync slaves to stop when work packages are empty and wait
   for(size_t i = 0; i < fileSyncSlaveVec.size(); i++)
   {
      if (likely(shallAbort.read() == 0))
         fileSyncSlaveVec[i]->setOnlyTerminateIfIdle(true);
      else
         fileSyncSlaveVec[i]->setOnlyTerminateIfIdle(false);

      fileSyncSlaveVec[i]->selfTerminate();
   }

   for(size_t i = 0; i < dirSyncSlaveVec.size(); i++)
   {
      if (likely(shallAbort.read() == 0))
         dirSyncSlaveVec[i]->setOnlyTerminateIfIdle(true);
      else
         dirSyncSlaveVec[i]->setOnlyTerminateIfIdle(false);

      dirSyncSlaveVec[i]->selfTerminate();
   }

   joinSyncSlaves();

cleanup:
   // wait for gather slaves to stop
   for(BuddyResyncerGatherSlaveVecIter iter = gatherSlaveVec.begin();
      iter != gatherSlaveVec.end(); iter++)
   {
      BuddyResyncerGatherSlave* slave = *iter;
      if(slave)
      {
         std::lock_guard<Mutex> safeLock(slave->statusMutex);
         while (slave->isRunning)
            slave->isRunningChangeCond.wait(&(slave->statusMutex));
      }
   }

   bool syncErrors = false;

   // wait for sync slaves to stop and save if any errors occured
   for(BuddyResyncerFileSyncSlaveVecIter iter = fileSyncSlaveVec.begin();
      iter != fileSyncSlaveVec.end(); iter++)
   {
      BuddyResyncerFileSyncSlave* slave = *iter;
      if(slave)
      {
         {
            std::lock_guard<Mutex> safeLock(slave->statusMutex);
            while (slave->isRunning)
               slave->isRunningChangeCond.wait(&(slave->statusMutex));
         }

         if (slave->getErrorCount() != 0)
            syncErrors = true;
      }
   }

   for(BuddyResyncerDirSyncSlaveVecIter iter = dirSyncSlaveVec.begin();
      iter != dirSyncSlaveVec.end(); iter++)
   {
      BuddyResyncerDirSyncSlave* slave = *iter;
      if(slave)
      {
         {
            std::lock_guard<Mutex> safeLock(slave->statusMutex);
            while (slave->isRunning)
               slave->isRunningChangeCond.wait(&(slave->statusMutex));
         }

         if (slave->getErrorCount() != 0)
            syncErrors = true;
      }
   }

   if (getStatus() == BuddyResyncJobState_RUNNING) // status not set to anything special
   {                                                // (e.g. FAILURE)
      if (shallAbort.read() != 0) // job aborted?
      {
         setStatus(BuddyResyncJobState_INTERRUPTED);
         informBuddy();
      }
      else if (syncErrors || targetWasOffline.read()) // any sync errors or success?
      {
         // we must set the buddy BAD if it has been offline during any period of time during which
         // the resync was also running. we implicitly do this during resync proper, since resync
         // slaves abort with errors if the target is offline. if the target goes offline *after*
         // the last proper resync messages has been sent and comes *back* before we try to inform
         // it we will never detect that it has been offline at all. concurrently executing
         // messages (eg TruncFile) may run between our opportunities to detect the offline state
         // and may fail to forward their actions *even though they should forward*. this would
         // lead to an inconsistent secondary. since the target has gone offline, the only
         // reasonable course of action is to fail to resync entirely.
         setStatus(BuddyResyncJobState_ERRORS);
         informBuddy();
      }
      else
      {
         setStatus(BuddyResyncJobState_SUCCESS);
         // unset timestamp override file if an override was set
         target.setLastBuddyComm(std::chrono::system_clock::from_time_t(0), true);
         // so the target went offline between the previous check "syncErrors || targetWasOffline".
         // any message that has tried to forward itself in the intervening time will have seen the
         // offline state, but will have been unable to set the buddy to needs-resync because it
         // still *is* needs-resync. the resync itself has been perfectly successful, but we have
         // to start another one anyway once the target comes back to ensure that no information
         // was lost.
         target.setBuddyNeedsResync(targetWasOffline.read());
         informBuddy();

         if (targetWasOffline.read())
            LOG(MIRRORING, WARNING,
                  "Resync successful, but target went offline during finalization. "
                  "Setting target to needs-resync again.", targetID);
      }
   }

   target.setBuddyResyncInProgress(false);
   endTime = time(NULL);
}

void BuddyResyncJob::abort()
{
   shallAbort.set(1); // tell the file walk in this class to abort

   // set setOnlyTerminateIfIdle on the slaves to false; they will be stopped by the main loop then
   for(BuddyResyncerGatherSlaveVecIter iter = gatherSlaveVec.begin(); iter != gatherSlaveVec.end();
      iter++)
   {
      BuddyResyncerGatherSlave* slave = *iter;
      if(slave)
      {
         slave->setOnlyTerminateIfIdle(false);
      }
   }

   // stop sync slaves
   for(BuddyResyncerFileSyncSlaveVecIter iter = fileSyncSlaveVec.begin();
      iter != fileSyncSlaveVec.end(); iter++)
   {
      BuddyResyncerFileSyncSlave* slave = *iter;
      if(slave)
      {
         slave->setOnlyTerminateIfIdle(false);
      }
   }

   for(BuddyResyncerDirSyncSlaveVecIter iter = dirSyncSlaveVec.begin();
      iter != dirSyncSlaveVec.end(); iter++)
   {
      BuddyResyncerDirSyncSlave* slave = *iter;
      if(slave)
      {
         slave->setOnlyTerminateIfIdle(false);
      }
   }
}

bool BuddyResyncJob::startGatherSlaves(const StorageTarget& target)
{
   // create a gather slaves if they don't exist yet and start them
   for (size_t i = 0; i < gatherSlaveVec.size(); i++)
   {
      if(!gatherSlaveVec[i])
         gatherSlaveVec[i] = new BuddyResyncerGatherSlave(target, &syncCandidates,
            &gatherSlavesWorkQueue, i);

      try
      {
         gatherSlaveVec[i]->resetSelfTerminate();
         gatherSlaveVec[i]->start();
         gatherSlaveVec[i]->setIsRunning(true);
      }
      catch (PThreadCreateException& e)
      {
         LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what());

         return false;
      }
   }

   return true;
}

bool BuddyResyncJob::startSyncSlaves()
{
   // create sync slaves and start them
   for(size_t i = 0; i < fileSyncSlaveVec.size(); i++)
   {
      if(!fileSyncSlaveVec[i])
         fileSyncSlaveVec[i] = new BuddyResyncerFileSyncSlave(targetID, &syncCandidates, i);

      try
      {
         fileSyncSlaveVec[i]->resetSelfTerminate();
         fileSyncSlaveVec[i]->start();
         fileSyncSlaveVec[i]->setIsRunning(true);
      }
      catch (PThreadCreateException& e)
      {
         LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what());

         // stop already started sync slaves
         for(size_t j = 0; j < i; j++)
            fileSyncSlaveVec[j]->selfTerminate();

         return false;
      }
   }

   for(size_t i = 0; i < dirSyncSlaveVec.size(); i++)
   {
      if(!dirSyncSlaveVec[i])
         dirSyncSlaveVec[i] = new BuddyResyncerDirSyncSlave(targetID, &syncCandidates, i);

      try
      {
         dirSyncSlaveVec[i]->resetSelfTerminate();
         dirSyncSlaveVec[i]->start();
         dirSyncSlaveVec[i]->setIsRunning(true);
      }
      catch (PThreadCreateException& e)
      {
         LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what());

         // stop already started sync slaves
         for (size_t j = 0; j < fileSyncSlaveVec.size(); j++)
            fileSyncSlaveVec[j]->selfTerminate();

         for (size_t j = 0; j < i; j++)
            dirSyncSlaveVec[j]->selfTerminate();

         return false;
      }
   }

   return true;
}

void BuddyResyncJob::joinGatherSlaves()
{
   for (size_t i = 0; i < gatherSlaveVec.size(); i++)
      gatherSlaveVec[i]->join();
}

void BuddyResyncJob::joinSyncSlaves()
{
   for (size_t i = 0; i < fileSyncSlaveVec.size(); i++)
      fileSyncSlaveVec[i]->join();

   for (size_t i = 0; i < dirSyncSlaveVec.size(); i++)
      dirSyncSlaveVec[i]->join();
}

void BuddyResyncJob::getJobStats(StorageBuddyResyncJobStatistics& outStats)
{
   uint64_t discoveredFiles = 0;
   uint64_t matchedFiles = 0;
   uint64_t discoveredDirs = numDirsDiscovered.read();
   uint64_t matchedDirs = numDirsMatched.read();
   uint64_t syncedFiles = 0;
   uint64_t syncedDirs = 0;
   uint64_t errorFiles = 0;
   uint64_t errorDirs = 0;

   for(size_t i = 0; i < gatherSlaveVec.size(); i++)
   {
      BuddyResyncerGatherSlave* slave = gatherSlaveVec[i];
      if(slave)
      {
         uint64_t tmpDiscoveredFiles = 0;
         uint64_t tmpMatchedFiles = 0;
         uint64_t tmpDiscoveredDirs = 0;
         uint64_t tmpMatchedDirs = 0;
         slave->getCounters(tmpDiscoveredFiles, tmpMatchedFiles, tmpDiscoveredDirs, tmpMatchedDirs);

         discoveredFiles += tmpDiscoveredFiles;
         matchedFiles += tmpMatchedFiles;
         discoveredDirs += tmpDiscoveredDirs;
         matchedDirs += tmpMatchedDirs;
      }
   }

   for(size_t i = 0; i < fileSyncSlaveVec.size(); i++)
   {
      BuddyResyncerFileSyncSlave* slave = fileSyncSlaveVec[i];
      if(slave)
      {
         syncedFiles += slave->getNumChunksSynced();
         errorFiles += slave->getErrorCount();
      }
   }

   for (size_t i = 0; i < dirSyncSlaveVec.size(); i++)
   {
      BuddyResyncerDirSyncSlave* slave = dirSyncSlaveVec[i];
      if (slave)
      {
         syncedDirs += slave->getNumDirsSynced();
         discoveredDirs += slave->getNumAdditionalDirsMatched();
         matchedDirs += slave->getNumAdditionalDirsMatched();
         errorDirs += slave->getErrorCount();
      }
   }

   outStats = StorageBuddyResyncJobStatistics(status, startTime, endTime, discoveredFiles,
         discoveredDirs, matchedFiles, matchedDirs, syncedFiles, syncedDirs, errorFiles, errorDirs);
}

void BuddyResyncJob::informBuddy()
{
   App* app = Program::getApp();
   NodeStore* storageNodes = app->getStorageNodes();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();
   TargetMapper* targetMapper = app->getTargetMapper();

   BuddyResyncJobState status = getStatus();
   TargetConsistencyState newTargetState;
   if ( (status == BuddyResyncJobState_ERRORS) || (status == BuddyResyncJobState_INTERRUPTED))
      newTargetState = TargetConsistencyState_BAD;
   else
   if (status == BuddyResyncJobState_SUCCESS)
      newTargetState = TargetConsistencyState_GOOD;
   else
   {
      LogContext(__func__).log(Log_NOTICE, "Refusing to set a state for buddy target, because "
         "resync status isn't well-defined. "
         "localTargetID: " + StringTk::uintToStr(targetID) + "; "
         "resyncState: " + StringTk::intToStr(status));
      return;
   }

   uint16_t buddyTargetID = buddyGroupMapper->getBuddyTargetID(targetID);
   NumNodeID buddyNodeID = targetMapper->getNodeID(buddyTargetID);
   auto storageNode = storageNodes->referenceNode(buddyNodeID);

   if (!storageNode)
   {
      LogContext(__func__).logErr(
         "Unable to inform buddy about finished resync. TargetID: " + StringTk::uintToStr(targetID)
            + "; buddyTargetID: " + StringTk::uintToStr(buddyTargetID) + "; buddyNodeID: "
            + buddyNodeID.str() + "; error: unknown storage node");
      return;
   }

   SetTargetConsistencyStatesRespMsg* respMsgCast;
   FhgfsOpsErr result;
   UInt16List targetIDs;
   UInt8List states;

   targetIDs.push_back(buddyTargetID);
   states.push_back(newTargetState);

   SetTargetConsistencyStatesMsg msg(NODETYPE_Storage, &targetIDs, &states, false);

   const auto respMsg = MessagingTk::requestResponse(*storageNode, msg,
         NETMSGTYPE_SetTargetConsistencyStatesResp);
   if (!respMsg)
   {
      LogContext(__func__).logErr(
         "Unable to inform buddy about finished resync. "
         "targetID: " + StringTk::uintToStr(targetID) + "; "
         "buddyTargetID: " + StringTk::uintToStr(buddyTargetID) + "; "
         "buddyNodeID: " + buddyNodeID.str() + "; "
         "error: Communication error");
      return;
   }

   respMsgCast = (SetTargetConsistencyStatesRespMsg*) respMsg.get();
   result = respMsgCast->getResult();

   if(result != FhgfsOpsErr_SUCCESS)
   {
      LogContext(__func__).logErr(
         "Error while informing buddy about finished resync. "
         "targetID: " + StringTk::uintToStr(targetID) + "; "
         "buddyTargetID: " + StringTk::uintToStr(buddyTargetID) + "; "
         "buddyNodeID: " + buddyNodeID.str() + "; "
         "error: " + boost::lexical_cast<std::string>(result));
   }
}

/*
 * check the CONFIG_BUDDYMIRROR_SUBDIR_NAME directory
 */
bool BuddyResyncJob::checkTopLevelDir(std::string& path, int64_t lastBuddyCommTimeSecs)
{
   struct stat statBuf;
   int statRes = stat(path.c_str(), &statBuf);

   if(statRes != 0)
   {
      LogContext(__func__).log(Log_WARNING,
         "Couldn't stat chunks directory; resync job can't run. targetID: "
         + StringTk::uintToStr(targetID) + "; path: " + path
         + "; Error: " + System::getErrString(errno));

      return false;
   }

   numDirsDiscovered.increase();
   int64_t dirMTime = (int64_t) statBuf.st_mtim.tv_sec;
   if(dirMTime > lastBuddyCommTimeSecs)
   { // sync candidate
      ChunkSyncCandidateDir candidate("", targetID);
      syncCandidates.add(candidate, this);
      numDirsMatched.increase();
   }

   return true;
}

/*
 * recursively walk through buddy mir directory until a depth of BUDDYRESYNCJOB_MAXDIRWALKDEPTH is
 * reached; everything with a greater depth gets passed to the GatherSlaves to work on it in
 * parallel
 */
bool BuddyResyncJob::walkDirs(std::string chunksPath, std::string relPath, int level,
   int64_t lastBuddyCommTimeSecs)
{
   bool retVal = true;

   DIR* dirHandle;
   struct dirent* dirEntry;

   dirHandle = opendir(std::string(chunksPath + "/" + relPath).c_str());

   if(!dirHandle)
   {
      LogContext(__func__).logErr("Unable to open path. "
         "targetID: " + StringTk::uintToStr(targetID) + "; "
         "Rel. path: " + relPath + "; "
         "Error: " + System::getErrString(errno) );
      return false;
   }

   while ((dirEntry = StorageTk::readdirFiltered(dirHandle)) != NULL)
   {
      if(shallAbort.read() != 0)
         break;

      // get stat info
      std::string currentRelPath;
      if(unlikely(relPath.empty()))
         currentRelPath = dirEntry->d_name;
      else
         currentRelPath = relPath + "/" + dirEntry->d_name;

      std::string currentFullPath = chunksPath + "/" + currentRelPath;
      struct stat statBuf;
      int statRes = stat(currentFullPath.c_str(), &statBuf);

      if(statRes != 0)
      {
         LogContext(__func__).log(Log_WARNING,
            "Couldn't stat directory, which was discovered previously. Resync job might not be "
               "complete. targetID " + StringTk::uintToStr(targetID) + "; "
               "Rel. path: " + relPath + "; "
               "Error: " + System::getErrString(errno));

         retVal = false;

         break; // => one error aborts it all
      }

      if(S_ISDIR(statBuf.st_mode))
      {
         // if level of dir is smaller than max, take care of it and recurse into it
         if(level < BUDDYRESYNCJOB_MAXDIRWALKDEPTH)
         {
            numDirsDiscovered.increase();
            int64_t dirMTime = (int64_t) statBuf.st_mtim.tv_sec;
            if(dirMTime > lastBuddyCommTimeSecs)
            { // sync candidate
               ChunkSyncCandidateDir candidate(currentRelPath, targetID);
               syncCandidates.add(candidate, this);
               numDirsMatched.increase();
            }

            bool walkRes = walkDirs(chunksPath, currentRelPath, level+1, lastBuddyCommTimeSecs);

            if (!walkRes)
               retVal = false;
         }
         else
            // otherwise pass it to the slaves; NOTE: gather slave takes full path
            gatherSlavesWorkQueue.add(currentFullPath, this);
      }
      else
      {
         LOG_DEBUG(__func__, Log_WARNING, "Found a file in directory structure");
      }
   }

   if(!dirEntry && errno) // error occured
   {
      LogContext(__func__).logErr(
         "Unable to read all directories; chunksPath: " + chunksPath + "; relativePath: " + relPath
            + "; SysErr: " + System::getErrString(errno));

      retVal = false;
   }

   int closedirRes = closedir(dirHandle);
   if (closedirRes != 0)
      LOG_DEBUG(__func__, Log_WARNING,
         "Unable to open path. targetID " + StringTk::uintToStr(targetID) + "; Rel. path: "
         + relPath + "; Error: " + System::getErrString(errno));

   return retVal;
}
