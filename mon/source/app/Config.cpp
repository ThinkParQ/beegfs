#include <common/toolkit/StringTk.h>
#include "Config.h"

#include <sys/stat.h>

#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-mon.conf"

Config::Config(int argc, char** argv): AbstractConfig(argc, argv)
{
   initConfig(argc, argv, true);

   // check mandatory value
   if(getSysMgmtdHost().empty())
      throw InvalidConfigException("Management host undefined.");

   // Load auth config file
   if (!dbAuthFile.empty())
   {
      std::ifstream authConfig(dbAuthFile);

      if (!authConfig.good())
         throw InvalidConfigException("Could not open InfluxDB authentication file");

      StringMap authMap;
      MapTk::loadStringMapFromFile(dbAuthFile.c_str(), &authMap);

      for (const auto& e : authMap) {
         if (e.first == "password") {
            dbAuthPassword = e.second;
         } else if (e.first == "username") {
            dbAuthUsername = e.second;
         } else if (e.first == "organization") {
            dbAuthOrg = e.second;
         } else if (e.first == "token") {
            dbAuthToken = e.second;
         } else {
            throw InvalidConfigException("The InfluxDB authentication file may only contain "
                  "the options username and password for influxdb version 1.x "
		            "organization and token for influxdb version 2.x" );
         }
      }
   }
}

void Config::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults();

   // re-definitions
   configMapRedefine("cfgFile",                    "");
   configMapRedefine("connUseRDMA",                "false");

   // own definitions
   configMapRedefine("connInterfacesFile",         "");
   configMapRedefine("tuneNumWorkers",             "4");
   configMapRedefine("runDaemonized",              "false");
   configMapRedefine("pidFile",                    "");

   configMapRedefine("dbType",                     "influxdb");
   configMapRedefine("dbHostName",                 "localhost");
   configMapRedefine("dbHostPort",                 "8086");
   configMapRedefine("dbDatabase",                 "beegfs_mon");
   configMapRedefine("dbAuthFile",                 "");

   // those are used by influxdb only but are kept like this for compatibility
   configMapRedefine("dbMaxPointsPerRequest",      "5000");
   configMapRedefine("dbSetRetentionPolicy",       "true");
   configMapRedefine("dbRetentionDuration",        "1d");

   configMapRedefine("dbBucket",                   "");

   configMapRedefine("cassandraMaxInsertsPerBatch","25");
   configMapRedefine("cassandraTTLSecs", "86400");

   configMapRedefine("collectClientOpsByNode",     "true");
   configMapRedefine("collectClientOpsByUser",     "true");

   configMapRedefine("httpTimeoutMSecs",           "1000");
   configMapRedefine("statsRequestIntervalSecs",   "5");
   configMapRedefine("nodelistRequestIntervalSecs","30");

   configMapRedefine("curlCheckSSLCertificates",   "true");

}

void Config::applyConfigMap(bool enableException, bool addDashes)
{
   AbstractConfig::applyConfigMap(false);

   for (StringMapIter iter = configMap.begin(); iter != configMap.end(); )
   {
      bool unknownElement = false;

      if (iter->first == std::string("logType"))
      {
         if (iter->second == "syslog")
         {
            logType = LogType_SYSLOG;
         }
         else if (iter->second == "logfile")
         {
            logType =  LogType_LOGFILE;
         }
         else
         {
            throw InvalidConfigException("The value of config argument logType is invalid:"
                  " Must be syslog or logfile.");
         }
      }
      else if (iter->first == std::string("connInterfacesFile"))
         connInterfacesFile = iter->second;
      else
      if (iter->first == std::string("tuneNumWorkers"))
         tuneNumWorkers = StringTk::strToUInt(iter->second);
      else
      if (iter->first == std::string("runDaemonized"))
         runDaemonized = StringTk::strToBool(iter->second);
      else
      if (iter->first == std::string("pidFile"))
         pidFile = iter->second;
      else
      if (iter->first == std::string("dbType"))
      {
         if (iter->second == "influxdb")
            dbType = DbTypes::INFLUXDB;
         else if (iter->second == "influxdb2")
            dbType = DbTypes::INFLUXDB2;
         else if (iter->second == "cassandra")
            dbType = DbTypes::CASSANDRA;
         else
            throw InvalidConfigException("The value of config argument dbType is invalid:"
                  " Must be influxdb or cassandra.");
      }
      else
      if (iter->first == std::string("dbHostName"))
         dbHostName = iter->second;
      else
      if (iter->first == std::string("dbHostPort"))
         dbHostPort = StringTk::strToUInt(iter->second);
      else
      if (iter->first == std::string("dbDatabase"))
         dbDatabase = iter->second;
      else
      if (iter->first == std::string("dbAuthFile"))
         dbAuthFile = iter->second;
      else
      // those are used by influxdb only but are kept like this for compatibility
      if (iter->first == std::string("dbMaxPointsPerRequest"))
         influxdbMaxPointsPerRequest = StringTk::strToUInt(iter->second);
      else
      if (iter->first == std::string("dbSetRetentionPolicy"))
         influxdbSetRetentionPolicy = StringTk::strToBool(iter->second);
      else
      if (iter->first == std::string("dbRetentionDuration"))
         influxdbRetentionDuration = iter->second;
      else
      // those are used by influxdb2
      if (iter->first == std::string("dbBucket"))
         dbBucket = iter->second;
      else

      if (iter->first == std::string("cassandraMaxInsertsPerBatch"))
         cassandraMaxInsertsPerBatch = StringTk::strToUInt(iter->second);
      else
      if (iter->first == std::string("cassandraTTLSecs"))
         cassandraTTLSecs = StringTk::strToUInt(iter->second);
      else
      if (iter->first == std::string("collectClientOpsByNode"))
         collectClientOpsByNode = StringTk::strToBool(iter->second);
      else
      if (iter->first == std::string("collectClientOpsByUser"))
         collectClientOpsByUser = StringTk::strToBool(iter->second);
      else
      if (iter->first == std::string("httpTimeoutMSecs"))
         httpTimeout = std::chrono::milliseconds(StringTk::strToUInt(iter->second));
      else
      if (iter->first == std::string("statsRequestIntervalSecs"))
         statsRequestInterval = std::chrono::seconds(StringTk::strToUInt(iter->second));
      else
      if (iter->first == std::string("nodelistRequestIntervalSecs"))
         nodelistRequestInterval = std::chrono::seconds(StringTk::strToUInt(iter->second));
      else
      if (iter->first == std::string("curlCheckSSLCertificates"))
         curlCheckSSLCertificates = StringTk::strToBool(iter->second);
      else
      {
         unknownElement = true;

         if (enableException)
         {
            throw InvalidConfigException(std::string("The config argument '")
                  + iter->first + std::string("' is invalid.") );
         }
      }

      if (unknownElement)
      {
         iter++;
      }
      else
      {
         iter = eraseFromConfigMap(iter);
      }
   }
}

void Config::initImplicitVals()
{
   AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);
}
