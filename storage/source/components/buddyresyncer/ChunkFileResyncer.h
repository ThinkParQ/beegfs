#pragma once

#include <common/nodes/Node.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/PThread.h>
#include <storage/StorageTargets.h>
#include <mutex>

class ChunkFileResyncer : public PThread   
{
   friend class BuddyResyncer; // (to grant access to internal mutex)     
   friend class BuddyResyncJob; // (to grant access to internal mutex)
   friend class ChunkBalancerJob;  // (to grant access to internal mutex)

   public:
      ChunkFileResyncer(uint16_t targetID, 
         uint8_t slaveID);
      virtual ~ChunkFileResyncer();

   protected: 

      enum ChunkFileResyncerMode
      {
         CHUNKFILERESYNCER_FLAG_BUDDYMIRROR = 0,
         CHUNKFILERESYNCER_FLAG_CHUNKBALANCE = 1,
         CHUNKFILERESYNCER_FLAG_CHUNKBALANCE_MIRRORED = 2,
      };

      AtomicSizeT onlyTerminateIfIdle; // atomic quasi-bool
      Mutex statusMutex; // protects isRunning
      Condition isRunningChangeCond;
      ChunkFileResyncerMode chunkFileResyncerMode;

      AtomicUInt64 numChunksSynced;
      AtomicUInt64 errorCount;

      bool isRunning; // true if an instance of this component is currently running

      uint16_t targetID;
      uint16_t destinationTargetID;
      std::string chunkPathStr;
      int fd;
      void run();
      virtual void syncLoop()=0;
      virtual int getFD(const std::unique_ptr<StorageTarget> & target)=0;

      bool removeChunkUnlocked(Node& node, uint16_t localTargetID, uint16_t destinationTargetID, std::string& pathStr);
      FhgfsOpsErr doResync(std::string& chunkPathStr, uint16_t localTargetID,
         uint16_t destinationTargetID, ChunkFileResyncerMode chunkFileResyncerMode);

   public:

      void setOnlyTerminateIfIdle(bool value)
      {
         if (value)
            onlyTerminateIfIdle.set(1);
         else
            onlyTerminateIfIdle.setZero();
      }

      bool getOnlyTerminateIfIdle()
      {
         if (onlyTerminateIfIdle.read() == 0)
            return false;
         else
            return true;
      }

      uint64_t getNumChunksSynced()
      {
         return numChunksSynced.read();
      }

      uint64_t getErrorCount()
      {
         return errorCount.read();
      }

      uint64_t getIsRunning()
      {
         const std::lock_guard<Mutex> lock(statusMutex);
         return this->isRunning;
      }

   // getters & setters

   protected:

      void setIsRunning(bool isRunning)
      {
         const std::lock_guard<Mutex> lock(statusMutex);

         this->isRunning = isRunning;
         isRunningChangeCond.broadcast();
      }
      
      bool getSelfTerminateNotIdle()
      {
         return ( (getSelfTerminate() && (!getOnlyTerminateIfIdle())) );
      }
};


