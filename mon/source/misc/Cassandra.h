#ifndef CASSANDRA_H_
#define CASSANDRA_H_

#include <common/nodes/NodeType.h>
#include <common/threading/Mutex.h>
#include <nodes/MetaNodeEx.h>
#include <nodes/StorageNodeEx.h>
#include <misc/TSDatabase.h>

#include <cassandra.h>
#include <dlfcn.h>

class Cassandra : public TSDatabase
{
   public:

      struct Config
      {
         std::string host;
         int port;
         std::string database;
         unsigned maxInsertsPerBatch;
         unsigned TTLSecs;
      };

      Cassandra(Config config);
      virtual ~Cassandra() {};

      virtual void insertMetaNodeData(
            std::shared_ptr<Node> node, const MetaNodeDataContent& data) override;
      virtual void insertStorageNodeData(
            std::shared_ptr<Node> node, const StorageNodeDataContent& data) override;
      virtual void insertHighResMetaNodeData(
            std::shared_ptr<Node> node, const HighResolutionStats& data) override;
      virtual void insertHighResStorageNodeData(
            std::shared_ptr<Node> node, const HighResolutionStats& data) override;
      virtual void insertStorageTargetsData(
            std::shared_ptr<Node> node, const StorageTargetInfo& data) override;
      virtual void insertClientNodeData(
            const std::string& id, const NodeType nodeType,
            const std::map<std::string, uint64_t>& opMap, bool perUser) override;
      virtual void write() override;

   private:
      std::function<decltype(cass_cluster_new)> cluster_new;
      std::function<decltype(cass_cluster_free)> cluster_free;
      std::function<decltype(cass_session_new)> session_new;
      std::function<decltype(cass_session_free)> session_free;
      std::function<decltype(cass_batch_new)> batch_new;
      std::function<decltype(cass_batch_free)> batch_free;
      std::function<decltype(cass_batch_add_statement)> batch_add_statement;
      std::function<decltype(cass_cluster_set_contact_points)> cluster_set_contact_points;
      std::function<decltype(cass_cluster_set_port)> cluster_set_port;
      std::function<decltype(cass_session_connect)> session_connect;
      std::function<decltype(cass_session_execute)> session_execute;
      std::function<decltype(cass_session_execute_batch)> session_execute_batch;
      std::function<decltype(cass_future_error_code)> future_error_code;
      std::function<decltype(cass_future_error_message)> future_error_message;
      std::function<decltype(cass_future_free)> future_free;
      std::function<decltype(cass_statement_new)> statement_new;
      std::function<decltype(cass_statement_free)> statement_free;

      std::unique_ptr<CassCluster, decltype(cluster_free)> cluster;
      std::unique_ptr<CassSession, decltype(session_free)> session;
      std::unique_ptr<CassBatch, decltype(batch_free)> batch;

      const Config config;
      std::unique_ptr<void, int(*)(void*)> libHandle;

      std::string queryBuffer;
      unsigned numQueries;

      mutable Mutex queryMutex;

      void appendQuery(const std::string& query);
      void query(const std::string& query, bool waitForResult = true);
      void writeUnlocked();
};

#endif
