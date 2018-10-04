#ifndef TS_DATABASE_H_
#define TS_DATABASE_H_

#include <common/nodes/NodeType.h>
#include <nodes/MetaNodeEx.h>
#include <nodes/StorageNodeEx.h>
#include <app/Config.h>

class TSDatabase
{
   public:
      static const unsigned connectionRetries = 3;

      TSDatabase() {};
      virtual ~TSDatabase() {};

      virtual void insertMetaNodeData(
            std::shared_ptr<Node> node, const MetaNodeDataContent& data) = 0;
      virtual void insertStorageNodeData(
            std::shared_ptr<Node> node, const StorageNodeDataContent& data) = 0;
      virtual void insertHighResMetaNodeData(
            std::shared_ptr<Node> node, const HighResolutionStats& data) = 0;
      virtual void insertHighResStorageNodeData(
            std::shared_ptr<Node> node, const HighResolutionStats& data) = 0;
      virtual void insertStorageTargetsData(
            std::shared_ptr<Node> node, const StorageTargetInfo& data) = 0;
      virtual void insertClientNodeData(
            const std::string& id, const NodeType nodeType,
            const std::map<std::string, uint64_t>& opMap, bool perUser) = 0;

      virtual void write() = 0;
};

#endif
