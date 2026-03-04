#include <common/toolkit/StringTk.h>
#include <program/Program.h>
#include <chrono>

#include "ChunkBalancerJob.h"

ChunkBalancerJob::ChunkBalancerJob() :
   PThread("ChunkBalancerJob")
   {}

ChunkBalancerJob::~ChunkBalancerJob()
{
   for (int i = static_cast<int>(ChunkSlaveVec.size()) - 1; i >= 0; i--)
   {
      SAFE_DELETE(ChunkSlaveVec[i]);  //delete slave from vector
      ChunkSlaveVec.erase(ChunkSlaveVec.begin() + i);
   }
   std::lock_guard<Mutex> mutexLock(jobStatsMutex);
   stats.reset();
}

void ChunkBalancerJob::run()
{
   const char* logContext = "ChunkBalancerJob running";
   int64_t emptyQueueDuration = 0;
   std::chrono::seconds iterationDuration = std::chrono::seconds::zero();
   std::chrono::steady_clock::time_point lastUpdateTime = std::chrono::steady_clock::now();
   // make sure only one job at a time can run!
   {
      std::lock_guard<Mutex> mutexLock(jobStatsMutex);
      
      if (stats.status == ChunkBalancerJobState_RUNNING)
      {
         LogContext(__func__).logErr("Refusing to run same ChunkBalancerJob twice!");
         return;
      }
      else
      {
         stats.status = ChunkBalancerJobState_RUNNING;
         //reset job stats
         stats.reset();
         ChunkSlaveVec.push_back(createSyncSlave(&syncCandidates, 0, &createSlaveRes)); //create first slave
                     
         if (!createSlaveRes)
         {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start first chunk balance slave.");
            stats.status = ChunkBalancerJobState_FAILURE;
            goto cleanup;
         }
         this->stats.workerNum ++;
      }
   }

   while (true)
   {
      uint64_t migratedChunksSum = 0;
      uint64_t slaveErrorsSum = 0;
      if (getSelfTerminate())
      {
         setStatus(ChunkBalancerJobState_INTERRUPTED);
         break;
      }
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         stats.workQueue = syncCandidates.getNumFiles();
      }
      if (stats.workQueue >=  CHUNKBALANCERJOB_MAX_FILE_PER_SLAVE_LIMIT) // check if queue per slave is full, if yes spawn more slave threads
      {
         emptyQueueDuration = 0;
         if (ChunkSlaveVec.size() >= CHUNKBALANCERJOB_MAX_SLAVE_LIMIT)  // check if number of slaves is max
         {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_SPAM,"Unable to start chunk balance slave; Number of slaves is equal to MAX_SLAVE_LIMIT.");
         }
         else
         {
            //start another chunk balance slave if queue is full and number of slaves is less then max
            ChunkSlaveVec.push_back(createSyncSlave(&syncCandidates, ChunkSlaveVec.size(), &createSlaveRes));
            
            if (!createSlaveRes)
            {
               //if failed, log a message 
               LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start additional chunk balance slave.");
            }
            incStatsWorker();
         }
         sleep(CHUNKBALANCERJOB_SLEEP_TIME); //sleep to wait for workQueue to decrease
      }
      else if (stats.workQueue ==0)  //check if queue is empty and increment timing counter
      {
         emptyQueueDuration += iterationDuration.count();
         setStatus(ChunkBalancerJobState_IDLE);
         if (emptyQueueDuration > CHUNKBALANCERJOB_MAX_TIME_LIMIT)  //if queue is empty for sufficient time, shut down
         {
            for (size_t i = 0; i < ChunkSlaveVec.size(); i++)
            {
               ChunkSlaveVec[i]->selfTerminate();
            }

            setStatus(ChunkBalancerJobState_SUCCESS);
            LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_NOTICE,"Marking ChunkBalancerJob as complete since no new work was queued for "+ std::to_string(CHUNKBALANCERJOB_MAX_TIME_LIMIT)+ "seconds. ");
            break;
         }
         //sleep since queue is empty
         bool itemsArrived = syncCandidates.waitForFilesWithResult(CHUNKBALANCERJOB_SLEEP_TIME * 1000);

         if (itemsArrived)
         {
            emptyQueueDuration = 0; // fresh batch arrived since we were idle
            setStatus(ChunkBalancerJobState_RUNNING);
         }
      }
      else   // normal operation when there are items in queue, reset timer
      {
         emptyQueueDuration = 0;
         setStatus(ChunkBalancerJobState_RUNNING);
         //iterate backwards to avoid index shifting when erasing elements
         for (int i = static_cast<int>(ChunkSlaveVec.size()) - 1; i >= 0; i--) 
         {   
            if (!(ChunkSlaveVec[i]->getIsRunning())) // check if slaves are running, if not, terminate them
            {
               LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_NOTICE,"Chunk Balance slave (ID:"+ std::to_string(i)+ ") is not running and will be terminated");
               ChunkSlaveVec[i]->selfTerminate(); 
               SAFE_DELETE(ChunkSlaveVec[i]);  //delete slave from vector 
               ChunkSlaveVec.erase(ChunkSlaveVec.begin() + i);
               decStatsWorker();
            }   
         }

         if (ChunkSlaveVec.empty())
         {
            ChunkSlaveVec.push_back(createSyncSlave(&syncCandidates, ChunkSlaveVec.size(), &createSlaveRes)); //create slave if there is not one running
            if (!createSlaveRes)
            {
               LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start chunk balance slave."); 
               setStatus(ChunkBalancerJobState_FAILURE);                
               goto cleanup;
            }
            incStatsWorker();
         }
      }
      //get stat updates on work done by slaves
      for (int i = static_cast<int>(ChunkSlaveVec.size()) - 1; i >= 0; i--)
      {
         ChunkBalancerFileSyncSlave* slave = ChunkSlaveVec[i];
         if (slave)
         {
            slaveErrorsSum += slave->getErrorCount();
            migratedChunksSum += slave->getNumChunksSynced();
         }
      }
      addStatsErrors(slaveErrorsSum);
      addStatsMigratedChunks(migratedChunksSum);
      //calculate time since last iteration
      std::chrono::steady_clock::time_point currTime = std::chrono::steady_clock::now();
      iterationDuration =  std::chrono::duration_cast<std::chrono::seconds>(currTime - lastUpdateTime);
      lastUpdateTime = currTime;
   }

cleanup:

   uint64_t migratedChunksSum = 0;
   uint64_t slaveErrorsSum = 0;
   // signal sync slaves to stop
   for (int i = static_cast<int>(ChunkSlaveVec.size()) - 1; i >= 0; i--)
   {
      if (ChunkSlaveVec[i])
         ChunkSlaveVec[i]->selfTerminate();
   }

   // wait for sync slaves to stop and save if any errors occured
   for (int i = static_cast<int>(ChunkSlaveVec.size()) - 1; i >= 0; i--)
   {
      ChunkBalancerFileSyncSlave* slave = ChunkSlaveVec[i];
      if (slave)
      {
         {
            std::lock_guard<Mutex> safeLock(slave->statusMutex);
            while (slave->isRunning)  
               slave->isRunningChangeCond.wait(&(slave->statusMutex));
         }

         slaveErrorsSum += slave->getErrorCount();
         migratedChunksSum += slave->getNumChunksSynced();
         SAFE_DELETE(slave);  //delete slave from vector
      }
      ChunkSlaveVec.erase(ChunkSlaveVec.begin() + i);
      decStatsWorker();
   }
   const auto status = getStatus();
   if (slaveErrorsSum > 0 && (status == ChunkBalancerJobState_SUCCESS
         || status == ChunkBalancerJobState_RUNNING
         || status == ChunkBalancerJobState_IDLE))
      setStatus(ChunkBalancerJobState_ERRORS);
   addStatsErrors(slaveErrorsSum);
   addStatsMigratedChunks(migratedChunksSum);
   std::lock_guard<Mutex> mutexLock(jobStatsMutex);
   stats.endTime = time(NULL);
}

ChunkBalancerFileSyncSlave*  ChunkBalancerJob::createSyncSlave(ChunkSyncCandidateStore* syncCandidates, uint8_t slaveID, bool* createSlaveResPtr)
{    
      ChunkBalancerFileSyncSlave* slave = new ChunkBalancerFileSyncSlave(0, syncCandidates, slaveID);
      try
      {
         slave->resetSelfTerminate();
         slave->start();
         slave->setIsRunning(true);
         *createSlaveResPtr = true;
      }
      catch (PThreadCreateException& e)
      {
         LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what());

         // stop already started sync slave
         slave->selfTerminate();
         *createSlaveResPtr = false;
      }
      return slave;
}


FhgfsOpsErr ChunkBalancerJob::addChunkSyncCandidate(ChunkSyncCandidateFile* candidate)
{
   App* app = Program::getApp();
   Config* config = app->getConfig();
   if (syncCandidates.getNumFiles() >=  config->getTuneChunkBalanceQueueLimit()) // check if total queue is full
   {
      LogContext(__func__).log(Log_WARNING, "Chunk Balancing queue is full, cannot add more chunks for balancing.");
      return FhgfsOpsErr_AGAIN;
   }
   syncCandidates.add(*candidate, this); 
   return FhgfsOpsErr_SUCCESS;
}


void ChunkBalancerJob::shutdown()
{
   setStatus(ChunkBalancerJobState_INTERRUPTED);
   this->selfTerminate(); //exit run() loop to enter cleanup
   syncCandidates.notifyFilesAdded(); // wake Job thread from sleep
}
