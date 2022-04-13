#ifndef STORAGEBENCHSLAVE_H_
#define STORAGEBENCHSLAVE_H_

#include <common/app/log/LogContext.h>
#include <common/benchmark/StorageBench.h>
#include <common/threading/Condition.h>
#include <common/threading/PThread.h>
#include <common/toolkit/Pipe.h>
#include <common/toolkit/TimeFine.h>
#include <common/Common.h>

#include <mutex>

// struct for the informations about a thread which simulates a client
struct StorageBenchThreadData
{
   uint16_t targetID;
   int targetThreadID;
   int64_t engagedSize; // amount of data which was submitted for write/read
   int fileDescriptor;
   int64_t neededTime;
};

// deleter functor for transferData
struct TransferDataDeleter {
   void operator()(char* transferData) { free(transferData); }
};

// map for the informations about a thread; key: virtual threadID, value: information about thread
typedef std::map<int, StorageBenchThreadData> StorageBenchThreadDataMap;
typedef StorageBenchThreadDataMap::iterator StorageBenchThreadDataMapIter;
typedef StorageBenchThreadDataMap::const_iterator StorageBenchThreadDataMapCIter;
typedef StorageBenchThreadDataMap::value_type StorageBenchThreadDataMapVal;



class StorageBenchSlave : public PThread
{
   public:
      StorageBenchSlave()
       : PThread("StorageBenchSlave"),
         threadCommunication(new Pipe(false, false) ),
         log("Storage Benchmark"),
         lastRunErrorCode(STORAGEBENCH_ERROR_NO_ERROR),
         status(StorageBenchStatus_UNINITIALIZED),
         benchType(StorageBenchType_NONE),
         blocksize(1),  // useless defaults
         size(1),       // useless defaults
         numThreads(1), // useless defaults
         numThreadsDone(0),
         targetIDs(NULL),
         transferData(nullptr)
      { }

      virtual ~StorageBenchSlave()
      {
         SAFE_DELETE(this->threadCommunication);
         SAFE_DELETE(this->targetIDs);
      }

      int initAndStartStorageBench(UInt16List* targetIDs, int64_t blocksize, int64_t size,
         int threads, bool odirect, StorageBenchType type);

      int cleanup(UInt16List* targetIDs);
      int stopBenchmark();
      StorageBenchStatus getStatusWithResults(UInt16List* targetIDs,
         StorageBenchResultsMap* outResults);
      void shutdownBenchmark();
      void waitForShutdownBenchmark();

   protected:

   private:
      Pipe* threadCommunication;
      Mutex statusMutex;
      Condition statusChangeCond;

      LogContext log;
      int lastRunErrorCode; // STORAGEBENCH_ERROR_...

      StorageBenchStatus status;
      StorageBenchType benchType;
      int64_t blocksize;
      int64_t size;
      int numThreads;
      bool odirect;
      unsigned int numThreadsDone;

      UInt16List* targetIDs;
      StorageBenchThreadDataMap threadData;
      std::unique_ptr<char[], TransferDataDeleter> transferData;

      TimeFine startTime;


      virtual void run();

      int initStorageBench(UInt16List* targetIDs, int64_t blocksize, int64_t size,
         int threads, bool odirect, StorageBenchType type);
      bool initTransferData(void);
      void initThreadData();
      void freeTransferData();

      bool checkReadData(void);
      bool createBenchmarkFolder(void);
      bool openFiles(void);
      bool closeFiles(void);

      int64_t getNextPackageSize(int threadID);
      int64_t getResult(uint16_t targetID);
      void getResults(UInt16List* targetIDs, StorageBenchResultsMap* outResults);
      void getAllResults(StorageBenchResultsMap* outResults);

      void setStatus(StorageBenchStatus newStatus)
      {
         const std::lock_guard<Mutex> lock(statusMutex);

         this->status = newStatus;
         this->statusChangeCond.broadcast();
      }

   public:
      //public inliners
      int getLastRunErrorCode()
      {
         return this->lastRunErrorCode;
      }

      StorageBenchStatus getStatus()
      {
         const std::lock_guard<Mutex> lock(statusMutex);

         return this->status;
      }

      StorageBenchType getType()
      {
         return this->benchType;
      }

      UInt16List* getTargetIDs()
      {
         return this->targetIDs;
      }
};

#endif /* STORAGEBENCHSLAVE_H_ */
