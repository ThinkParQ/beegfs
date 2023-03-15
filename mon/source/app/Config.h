#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>


class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv);

      enum DbTypes
      {
         INFLUXDB,
         INFLUXDB2,
         CASSANDRA
      };

   private:
      // configurables
      std::string connInterfacesFile;
      unsigned    tuneNumWorkers;
      bool        runDaemonized;
      std::string pidFile;

      // mon-specific configurables
      DbTypes dbType;
      std::string dbHostName;
      unsigned dbHostPort;
      std::string dbDatabase;
      std::string dbBucket;
      std::string dbAuthFile;
      unsigned influxdbMaxPointsPerRequest;
      bool influxdbSetRetentionPolicy;
      std::string influxdbRetentionDuration;
      unsigned cassandraMaxInsertsPerBatch;
      unsigned cassandraTTLSecs;
      bool collectClientOpsByNode;
      bool collectClientOpsByUser;
      std::chrono::milliseconds httpTimeout;
      std::chrono::seconds statsRequestInterval;
      std::chrono::seconds nodelistRequestInterval;
      bool curlCheckSSLCertificates;

      std::string dbAuthUsername;
      std::string dbAuthPassword;
      std::string dbAuthOrg;
      std::string dbAuthToken;


      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;

   public:
      // getters & setters

      const std::string& getConnInterfacesFile() const
      {
         return connInterfacesFile;
      }

      unsigned getTuneNumWorkers() const
      {
         return tuneNumWorkers;
      }

      bool getRunDaemonized() const
      {
         return runDaemonized;
      }

      const std::string& getPIDFile() const
      {
         return pidFile;
      }

      DbTypes getDbType() const
      {
         return dbType;
      }

      const std::string& getDbHostName() const
      {
         return dbHostName;
      }

      unsigned getDbHostPort() const
      {
         return dbHostPort;
      }

      const std::string& getDbDatabase() const
      {
         return dbDatabase;
      }

      const std::string& getDbBucket() const
      {
         return dbBucket;
      }

      unsigned getInfluxdbMaxPointsPerRequest() const
      {
         return influxdbMaxPointsPerRequest;
      }

      bool getInfluxDbSetRetentionPolicy() const
      {
         return influxdbSetRetentionPolicy;
      }

      const std::string& getInfluxDbRetentionDuration() const
      {
         return influxdbRetentionDuration;
      }

      unsigned getCassandraMaxInsertsPerBatch() const
      {
         return cassandraMaxInsertsPerBatch;
      }

      unsigned getCassandraTTLSecs() const
      {
         return cassandraTTLSecs;
      }

      bool getCollectClientOpsByNode() const
      {
         return collectClientOpsByNode;
      }

      bool getCollectClientOpsByUser() const
      {
         return collectClientOpsByUser;
      }

      const std::chrono::milliseconds& getHttpTimeout() const
      {
         return httpTimeout;
      }

      const std::chrono::seconds& getStatsRequestInterval() const
      {
         return statsRequestInterval;
      }

      const std::chrono::seconds& getNodelistRequestInterval() const
      {
         return nodelistRequestInterval;
      }

      const std::string& getDbAuthUsername() const
      {
         return dbAuthUsername;
      }

      const std::string& getDbAuthPassword() const
      {
         return dbAuthPassword;
      }

      const std::string& getDbAuthOrg() const
      {
         return dbAuthOrg;
      }

      const std::string& getDbAuthToken() const
      {
         return dbAuthToken;
      }

      bool getCurlCheckSSLCertificates() const
      {
         return curlCheckSSLCertificates;
      }
};

#endif /*CONFIG_H_*/
