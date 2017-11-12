#ifndef REQUESTSTORAGEDATAWORK_H_
#define REQUESTSTORAGEDATAWORK_H_

#include <common/components/worker/Work.h>
#include <common/nodes/ClientOps.h>
#include <common/storage/StorageTargetInfo.h>
#include <misc/TSDatabase.h>
#include <nodes/StorageNodeEx.h>

class StatsCollector;

class RequestStorageDataWork : public Work
{
   public:
      struct Result
      {
         std::shared_ptr<StorageNodeEx> node;
         StorageNodeDataContent data;
         HighResStatsList highResStatsList;
         StorageTargetInfoList storageTargetList;
         ClientOpsRequestor::IdOpsUnorderedMap idOpsUnorderedMap;
      };

      RequestStorageDataWork(std::shared_ptr<StorageNodeEx> node,
            StatsCollector* statsCollector) :
         node(std::move(node)),
         statsCollector(statsCollector)
      {}

      void process(char* bufIn, unsigned bufInLen, char* bufOut,
            unsigned bufOutLen);

   private:
      std::shared_ptr<StorageNodeEx> node;
      StatsCollector* statsCollector;
};

#endif /*REQUESTSTORAGEDATAWORK_H_*/
