#include "StorageBenchOperator.h"


int StorageBenchOperator::initAndStartStorageBench(UInt16List* targetIDs, int64_t blocksize,
   int64_t size, int threads, bool odirect, StorageBenchType type)
{
   return this->slave.initAndStartStorageBench(targetIDs, blocksize, size, threads, odirect, type);
}

int StorageBenchOperator::cleanup(UInt16List* targetIDs)
{
   return this->slave.cleanup(targetIDs);
}

int StorageBenchOperator::stopBenchmark()
{
   return this->slave.stopBenchmark();
}

StorageBenchStatus StorageBenchOperator::getStatusWithResults(UInt16List* targetIDs,
   StorageBenchResultsMap* outResults)
{
   return this->slave.getStatusWithResults(targetIDs, outResults);
}

void StorageBenchOperator::shutdownBenchmark()
{
   this->slave.shutdownBenchmark();
}

void StorageBenchOperator::waitForShutdownBenchmark()
{
   this->slave.waitForShutdownBenchmark();
}




