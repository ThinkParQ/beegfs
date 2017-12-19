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
         ClientOpsRequestor::IdOpsUnorderedMap ipOpsUnorderedMap;
         ClientOpsRequestor::IdOpsUnorderedMap userOpsUnorderedMap;
      };

      RequestStorageDataWork(std::shared_ptr<StorageNodeEx> node,
            StatsCollector* statsCollector, bool collectClientOpsByNode,
            bool collectClientOpsByUser) :
            node(std::move(node)),
            statsCollector(statsCollector),
            collectClientOpsByNode(collectClientOpsByNode),
            collectClientOpsByUser(collectClientOpsByUser)
      {}

      void process(char* bufIn, unsigned bufInLen, char* bufOut,
            unsigned bufOutLen);

   private:
      std::shared_ptr<StorageNodeEx> node;
      StatsCollector* statsCollector;
      bool collectClientOpsByNode;
      bool collectClientOpsByUser;
};

#endif /*REQUESTSTORAGEDATAWORK_H_*/
