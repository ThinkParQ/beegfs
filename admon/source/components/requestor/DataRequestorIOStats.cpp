#include "DataRequestorIOStats.h"
#include <common/toolkit/StringTk.h>
#include <program/Program.h>

void DataRequestorIOStats::run()
{
   try
   {
      log.log(Log_DEBUG, std::string("Component started."));

      registerSignalHandler();
      requestLoop();

      log.log(Log_DEBUG, std::string("Component stopped."));
   }
   catch (std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }
}

void DataRequestorIOStats::requestLoop()
{
   // do loop until terminate is set
   while (!getSelfTerminate())
   {
      // create work packets for the meta nodes; for each metadata node a RequestMetaDataWork is
      // put into the work queue, where it will be fetched by a worker
      for (const auto& node : metaNodes->referenceAllNodes())
         this->workQueue->addIndirectWork(
               new RequestMetaDataWork(std::static_pointer_cast<MetaNodeEx>(node)));

      // same for storage nodes
      for (const auto& node : storageNodes->referenceAllNodes())
         this->workQueue->addIndirectWork(
            new RequestStorageDataWork(std::static_pointer_cast<StorageNodeEx>(node)));

      // if stats for clients, etc. should be gathered, this would be the right place for it

      // clean up database once an hour to make sure it does not grow to much
      if (this->t.elapsedMS() > 3600000) // 1 hour
      {
         Program::getApp()->getDB()->cleanUp();
         this->t.setToNow();
      }

      // do nothing but wait for the time of queryInterval
      if (PThread::waitForSelfTerminateOrder(this->queryInterval))
         break;
   }
}
