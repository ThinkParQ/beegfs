#ifndef STORAGEBENCHOPERATOR_H_
#define STORAGEBENCHOPERATOR_H_


#include "StorageBenchSlave.h"


class StorageBenchOperator
{
   public:
      StorageBenchOperator() {}

      int initAndStartStorageBench(UInt16List* targetIDs, int64_t blocksize, int64_t size,
         int threads, bool odirect, StorageBenchType type);

      int cleanup(UInt16List* targetIDs);
      int stopBenchmark();
      StorageBenchStatus getStatusWithResults(UInt16List* targetIDs,
         StorageBenchResultsMap* outResults);
      void shutdownBenchmark();
      void waitForShutdownBenchmark();

   private:
      StorageBenchSlave slave;

   protected:

   public:
      // inliners

      StorageBenchStatus getStatus()
      {
         return this->slave.getStatus();
      }

      StorageBenchType getType()
      {
         return this->slave.getType();
      }

      int getLastRunErrorCode()
      {
         return this->slave.getLastRunErrorCode();
      }
};

#endif /* STORAGEBENCHOPERATOR_H_ */
