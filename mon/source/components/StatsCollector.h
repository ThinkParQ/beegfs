#ifndef STATSCOLLECTOR_H_
#define STATSCOLLECTOR_H_

#include <common/threading/PThread.h>
#include <components/worker/RequestMetaDataWork.h>
#include <components/worker/RequestStorageDataWork.h>
#include <common/nodes/ClientOps.h>

#include <mutex>
#include <condition_variable>

class App;

class StatsCollector : public PThread
{
   friend class RequestMetaDataWork;
   friend class RequestStorageDataWork;

   public:
      StatsCollector(App* app);

   private:
      App* const app;
      ClientOps ipMetaClientOps;
      ClientOps ipStorageClientOps;
      ClientOps userMetaClientOps;
      ClientOps userStorageClientOps;

      mutable std::mutex mutex;
      int workItemCounter;
      std::list<RequestMetaDataWork::Result> metaResults;
      std::list<RequestStorageDataWork::Result> storageResults;
      std::condition_variable condVar;

      virtual void run() override;
      void requestLoop();
      void processClientOps(ClientOps& clientOps, NodeType nodeType, bool perUser);

      void insertMetaData(RequestMetaDataWork::Result result)
      {
         const std::unique_lock<std::mutex> lock(mutex);
         metaResults.push_back(std::move(result));
         workItemCounter--;
         condVar.notify_one();
      }

      void insertStorageData(RequestStorageDataWork::Result result)
      {
         const std::unique_lock<std::mutex> lock(mutex);
         storageResults.push_back(std::move(result));
         workItemCounter--;
         condVar.notify_one();
      }
};

#endif /*STATSCOLLECTOR_H_*/
