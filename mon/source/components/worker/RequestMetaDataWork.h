#ifndef REQUESTMETADATAWORK_H_
#define REQUESTMETADATAWORK_H_

#include <common/components/worker/Work.h>
#include <common/nodes/ClientOps.h>
#include <misc/TSDatabase.h>
#include <nodes/MetaNodeEx.h>

class StatsCollector;

class RequestMetaDataWork : public Work
{
   public:
      struct Result
      {
         std::shared_ptr<MetaNodeEx> node;
         MetaNodeDataContent data;
         HighResStatsList highResStatsList;
         ClientOpsRequestor::IdOpsUnorderedMap ipOpsUnorderedMap;
         ClientOpsRequestor::IdOpsUnorderedMap userOpsUnorderedMap;
      };

      RequestMetaDataWork(std::shared_ptr<MetaNodeEx> node,
            StatsCollector* statsCollector,
            bool collectClientOpsByNode, bool collectClientOpsByUser) :
            node(std::move(node)),
            statsCollector(statsCollector),
            collectClientOpsByNode(collectClientOpsByNode),
            collectClientOpsByUser(collectClientOpsByUser)
      {}

      virtual void process(char* bufIn, unsigned bufInLen,
            char* bufOut, unsigned bufOutLen) override;

   private:
      std::shared_ptr<MetaNodeEx> node;
      StatsCollector* statsCollector;
      bool collectClientOpsByNode;
      bool collectClientOpsByUser;
};

#endif /*REQUESTMETADATAWORK_H_*/
