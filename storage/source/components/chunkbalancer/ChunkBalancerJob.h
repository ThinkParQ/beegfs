#pragma once

#include <components/buddyresyncer/SyncCandidate.h>
#include <common/storage/chunkbalancer/ChunkBalancerJobStatistics.h>
#include <components/chunkbalancer/ChunkBalancerFileSyncSlave.h>


/**
 * A storage chunk balancing job component. Starts, stops and controls the storage chunk balance slaves. 
 * Only one instance can run at a given time. 
 */

class ChunkBalancerJob : public PThread
{
   friend class CpChunkPathsMsgEx;

   public:
      ChunkBalancerJob();
      virtual ~ChunkBalancerJob();

      virtual void run();

      void abort();

   private:
      //set maximum parameters of slaves and queues
      static constexpr unsigned CHUNKBALANCERJOB_MAX_SLAVE_LIMIT = 4;
      static constexpr unsigned CHUNKBALANCERJOB_MAX_FILE_PER_SLAVE_LIMIT = 5000; // number of max items per slave thread
   
      static constexpr unsigned CHUNKBALANCERJOB_SLEEP_TIME = 5; //seconds of idle time before chunkbalancerJob checks status of queues again
      static constexpr unsigned CHUNKBALANCERJOB_MAX_TIME_LIMIT = 600;  //maximum time in seconds of empty queue before ChunkBalancerJob shuts itself and slaves down
      Mutex jobStatsMutex;


      ChunkSyncCandidateStore syncCandidates;

      ChunkBalancerFileSyncSlaveVec ChunkSlaveVec;

      AtomicInt16 shallAbort; // quasi-boolean
      AtomicInt16 targetWasOffline;

      bool createSlaveRes;
      ChunkBalancerFileSyncSlave* createSyncSlave(ChunkSyncCandidateStore* syncCandidates, uint8_t slaveID, bool* createSlaveResPtr);

      ChunkBalancerJobStatistics stats;

   public:

      void getJobStats(ChunkBalancerJobStatistics* outStats)
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         *outStats = stats;
      }

      ChunkBalancerJobState getStatus()
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         return this->stats.status;
      }

      bool isRunningStarting()
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         return this->stats.status == ChunkBalancerJobState_RUNNING || this->stats.status == ChunkBalancerJobState_STARTING || this->stats.status == ChunkBalancerJobState_IDLE;
      }

      FhgfsOpsErr addChunkSyncCandidate(ChunkSyncCandidateFile* candidate);
      void shutdown();

   private:
      void setStatus(ChunkBalancerJobState status)
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         this->stats.status = status;
      }

      void incStatsWorker()
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         this->stats.workerNum++;
      }


      void decStatsWorker()
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         this->stats.workerNum--;
      }


      void addStatsErrors(uint64_t slaveErrorCount)
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         stats.errorCount = slaveErrorCount;
      }

      void addStatsMigratedChunks(uint64_t slaveMigratedChunks)
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         stats.migratedChunks = slaveMigratedChunks;
      }

};
