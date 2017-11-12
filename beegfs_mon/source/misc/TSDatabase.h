#ifndef TS_DATABASE_H_
#define TS_DATABASE_H_

#include <common/nodes/NodeType.h>
#include <common/threading/Mutex.h>
#include <nodes/MetaNodeEx.h>
#include <nodes/StorageNodeEx.h>
#include <misc/CurlWrapper.h>
#include <app/Config.h>

class App;

class TSDatabase
{
   public:
      TSDatabase(Config* config);

      void setupDatabase() const;

      void insertMetaNodeData(std::shared_ptr<Node> node, const MetaNodeDataContent& data);
      void insertStorageNodeData(std::shared_ptr<Node> node, const StorageNodeDataContent& data);
      void insertHighResMetaNodeData(std::shared_ptr<Node> node, const HighResolutionStats& data);
      void insertHighResStorageNodeData(std::shared_ptr<Node> node,
            const HighResolutionStats& data);
      void insertStorageTargetsData(std::shared_ptr<Node> node, const StorageTargetInfo& data);

      void insertClientNodeData(const std::string& node, const NodeType nodeType,
            const std::map<std::string, uint64_t>& opMap);

      void writePoints();

      static std::string escapeStringForWrite(const std::string& str);

   private:
      Config* const config;

      std::unique_ptr<CurlWrapper> curlWrapper;

      std::string points;
      unsigned numPoints;
      unsigned maxPointsPerRequest;

      mutable Mutex pointsMutex;
      mutable Mutex curlMutex;

      std::string host;
      int port;
      std::string database;

      void appendPoint(const std::string& point);
      void writePointsUnlocked();
      void sendWrite(const std::string& data) const;
      void sendQuery(const std::string& data) const;
      bool sendPing() const;


};

#endif
