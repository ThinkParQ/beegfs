#pragma once

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <components/chunkbalancer/SyncCandidate.h>
#include <common/storage/chunkbalancer/ChunkBalancerJobStatistics.h>
#include <storage/GlobalInodeLockStore.h>
#include <atomic>
#include <mutex>

//forward declaration of meta slave
class ChunkBalancerMetaSlave;

/**
 * A metadata chunk balancing job component. Starts, stops and controls the metadata chunk balance slaves.
 * Only one instance can run at a given time.
 */
class ChunkBalancerJob : public PThread
{
   friend class StartChunkBalanceMsgEx;
   public:
      ChunkBalancerJob();
      virtual ~ChunkBalancerJob();

      virtual void run();

      void abort();

   private:
      //set maximum parameters of slaves and queues
      static constexpr unsigned CHUNKBALANCERJOB_MAX_SLAVE_LIMIT = 4;
      static constexpr unsigned CHUNKBALANCERJOB_MAX_FILE_PER_SLAVE_LIMIT = 5000; // number of max items per slave thread

      static constexpr unsigned CHUNKBALANCERJOB_SLEEP_TIME = 4; //seconds of idle time before chunkbalancerJob checks status of queues again


      GlobalInodeLockStore* inodeLockStore;   //inode file store to keep track of locked inodes

      Mutex jobStatsMutex;

      ChunkSyncCandidateStore copyCandidates;   //candidates to forward to storage for copy operation
      std::vector<ChunkBalancerMetaSlave*> ChunkSlaveVec;

      AtomicInt16 shallAbort; // quasi-boolean
      AtomicInt16 targetWasOffline;

      bool createSlaveRes;
      ChunkBalancerMetaSlave* createSyncSlave(ChunkSyncCandidateStore* copyCandidates,  uint8_t slaveID, bool* createSlaveRes);

      ChunkBalancerJobStatistics stats;

   public:


      ChunkBalancerJobState getStatus()
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         return this->stats.status;
      }

      bool isRunningStarting()
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         return  this->stats.status == ChunkBalancerJobState_RUNNING ||  this->stats.status == ChunkBalancerJobState_STARTING ||  this->stats.status == ChunkBalancerJobState_IDLE;
      }

      FhgfsOpsErr addChunkSyncCandidate(ChunkSyncCandidateFile* candidate);

      void shutdown();

      void getJobStats(ChunkBalancerJobStatistics* outStats)
      {
         std::lock_guard<Mutex> mutexLock(jobStatsMutex);
         *outStats = stats;
      }

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

