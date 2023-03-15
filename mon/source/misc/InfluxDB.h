#ifndef INFLUXDB_H_
#define INFLUXDB_H_

#include <common/nodes/NodeType.h>
#include <common/threading/Mutex.h>
#include <nodes/MetaNodeEx.h>
#include <nodes/StorageNodeEx.h>
#include <misc/CurlWrapper.h>
#include <misc/TSDatabase.h>
#include <app/Config.h>

enum InfluxDBVersion
{
   INFLUXDB,
   INFLUXDB2,
};

class App;

class InfluxDB : public TSDatabase
{
   public:

      struct Config
      {
         std::string host;
         int port;
         std::string database;
         std::chrono::milliseconds httpTimeout;
         unsigned maxPointsPerRequest;
         bool setRetentionPolicy;
         std::string retentionDuration;
         bool curlCheckSSLCertificates;
         std::string username;
         std::string password;
         std::string bucket;
         std::string organization;
         std::string token;
         InfluxDBVersion dbVersion;

      };

      InfluxDB(Config cfg);
      virtual ~InfluxDB() {};

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

      static std::string escapeStringForWrite(const std::string& str);

   private:
      const Config config;

      std::unique_ptr<CurlWrapper> curlWrapper;

      std::string points;
      unsigned numPoints = 0;

      mutable Mutex pointsMutex;
      mutable Mutex curlMutex;

      void setupDatabase() const;
      void appendPoint(const std::string& point);
      void writePointsUnlocked();
      void sendWrite(const std::string& data) const;
      void sendQuery(const std::string& data) const;
      bool sendPing() const;


};

#endif
