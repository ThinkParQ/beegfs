#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>

class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv) throw(InvalidConfigException);

   private:

      Mutex mgmtdMutex;

      // configurables

      std::string connInterfacesFile;

      bool        debugRunComponentThreads;
      bool        debugRunStartupTests;
      bool        debugRunComponentTests;
      bool        debugRunIntegrationTests;

      unsigned    tuneNumWorkers;

      bool        runDaemonized;

      std::string pidFile;

      // admon-specific configurables

      int         httpPort;

      int         queryInterval;
      int         reloadInterval;

      std::string databaseFile; // database file

      bool        clearDatabase; // possibility to erase the database file

      // eMail notification which overrides the runtime configuration
      bool mailEnabled;
      std::string mailSmtpServer;
      std::string mailSender;
      std::string mailRecipient;
      int mailMinDownTimeSec;
      int mailResendMailTimeMin;
      int mailCheckIntervalTimeSec;

      // internals

      virtual void loadDefaults(bool addDashes);
      virtual void applyConfigMap(bool enableException, bool addDashes)
         throw(InvalidConfigException);
      virtual void initImplicitVals() throw(InvalidConfigException);
      std::string createDefaultCfgFilename();



   public:
      // getters & setters

      std::string getConnInterfacesFile() const
      {
         return connInterfacesFile;
      }

      bool getDebugRunComponentThreads() const
      {
         return debugRunComponentThreads;
      }

      bool getDebugRunStartupTests() const
      {
         return debugRunStartupTests;
      }

      bool getDebugRunComponentTests() const
      {
         return debugRunComponentTests;
      }

      bool getDebugRunIntegrationTests() const
      {
         return debugRunIntegrationTests;
      }

      unsigned getTuneNumWorkers() const
      {
         return tuneNumWorkers;
      }

      bool getRunDaemonized() const
      {
         return runDaemonized;
      }

      std::string getPIDFile() const
      {
         return pidFile;
      }

      int getHttpPort() const
      {
         return httpPort;
      }

      int getQueryInterval() const
      {
         return queryInterval;
      }

      int getReloadInterval() const
      {
         return reloadInterval;
      }

      std::string getDatabaseFile() const
      {
         return databaseFile;
      }

      bool getClearDatabase() const
      {
         return clearDatabase;
      }

      std::string getSysMgmtdHostLocked()
      {
         SafeMutexLock mutexLock(&mgmtdMutex);

         std::string retVal = sysMgmtdHost;

         mutexLock.unlock();

         return retVal;
      }

      void setSysMgmtdHostLocked(std::string host)
      {
         SafeMutexLock mutexLock(&mgmtdMutex);

         sysMgmtdHost = host;

         mutexLock.unlock();
      }

      int getConnMgmtdPortTCPLocked()
      {
         int retVal = 0;

         SafeMutexLock mutexLock(&mgmtdMutex);

         retVal = connMgmtdPortTCP + connPortShift;

         mutexLock.unlock();

         return retVal;
      }

      void setConnMgmtdPortTCPLocked(int port)
      {
         SafeMutexLock mutexLock(&mgmtdMutex);

         connMgmtdPortTCP = port;

         mutexLock.unlock();
      }

      int getConnMgmtdPortUDPLocked()
      {
         int retVal = 0;

         SafeMutexLock mutexLock(&mgmtdMutex);

         retVal = connMgmtdPortUDP + connPortShift;

         mutexLock.unlock();

         return retVal;
      }

      void setConnMgmtdPortUDPLocked(int port)
      {
         SafeMutexLock mutexLock(&mgmtdMutex);

         connMgmtdPortUDP = port;

         mutexLock.unlock();
      }

      void resetMgmtdDaemon();

      bool getMailEnabled()
      {
         return mailEnabled;
      }

      std::string getMailSmtpServer()
      {
         return mailSmtpServer;
      }

      std::string getMailSender()
      {
         return mailSender;
      }

      std::string getMailRecipient()
      {
         return mailRecipient;
      }

      int getMailMinDownTimeSec()
      {
         return mailMinDownTimeSec;
      }

      int getMailResendMailTimeMin()
      {
         return mailResendMailTimeMin;
      }

      int getMailCheckIntervalTimeSec()
      {
         return mailCheckIntervalTimeSec;
      }

      bool isMailConfigOverrideActive()
      {
         return mailEnabled;
      }

};

#endif /*CONFIG_H_*/
