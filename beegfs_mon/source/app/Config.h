#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>


class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv);

   private:
      // configurables
      std::string connInterfacesFile;
      unsigned    tuneNumWorkers;
      bool        runDaemonized;
      std::string pidFile;

      // mon-specific configurables
      std::string dbHostName;
      unsigned dbHostPort;
      std::string dbDatabase;
      unsigned dbMaxPointsPerRequest;
      bool dbSetRetentionPolicy;
      std::string dbRetentionDuration;
      std::chrono::milliseconds httpTimeout;
      std::chrono::seconds statsRequestInterval;
      std::chrono::seconds nodelistRequestInterval;

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

      unsigned getDbMaxPointsPerRequest() const
      {
         return dbMaxPointsPerRequest;
      }

      bool getDbSetRetentionPolicy() const
      {
         return dbSetRetentionPolicy;
      }

      const std::string& getDbRetentionDuration() const
      {
         return dbRetentionDuration;
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
};

#endif /*CONFIG_H_*/
