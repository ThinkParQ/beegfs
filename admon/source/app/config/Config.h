#ifndef CONFIG_H_
#define CONFIG_H_


#include <common/app/config/AbstractConfig.h>



#define MAIL_SENDMAIL_DEFAULT_PATH         "sendmail"

enum SmtpSendType
{
   SmtpSendType_SOCKET = 0,
   SmtpSendType_SENDMAIL = 1,
};



class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv);

   private:
      mutable Mutex mgmtdMutex;

      // configurables

      std::string connInterfacesFile;

      bool        debugRunComponentThreads;

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
      std::string mailSmtpSendType;
      SmtpSendType mailSmtpSendTypeNum;  // auto-generated based on mailSmtpSendType
      std::string mailSendmailPath;
      std::string mailSmtpServer;
      std::string mailSender;
      std::string mailRecipient;
      int mailMinDownTimeSec;
      int mailResendMailTimeMin;
      int mailCheckIntervalTimeSec;

      // internals

      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;
      void initMailerConfig();
      std::string createDefaultCfgFilename() const;

   public:
      // getters & setters

      const std::string& getConnInterfacesFile() const
      {
         return connInterfacesFile;
      }

      bool getDebugRunComponentThreads() const
      {
         return debugRunComponentThreads;
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

      const std::string& getDatabaseFile() const
      {
         return databaseFile;
      }

      bool getClearDatabase() const
      {
         return clearDatabase;
      }

      const std::string& getSysMgmtdHostLocked() const
      {
         std::lock_guard<Mutex> lock(mgmtdMutex);
         return sysMgmtdHost;
      }

      void setSysMgmtdHostLocked(const std::string& host)
      {
         std::lock_guard<Mutex> lock(mgmtdMutex);
         sysMgmtdHost = host;
      }

      int getConnMgmtdPortTCPLocked() const
      {
         std::lock_guard<Mutex> lock(mgmtdMutex);
         return connMgmtdPortTCP + connPortShift;
      }

      void setConnMgmtdPortTCPLocked(int port)
      {
         std::lock_guard<Mutex> lock(mgmtdMutex);
         connMgmtdPortTCP = port;
      }

      int getConnMgmtdPortUDPLocked() const
      {
         std::lock_guard<Mutex> lock(mgmtdMutex);
         return connMgmtdPortUDP + connPortShift;
      }

      void setConnMgmtdPortUDPLocked(int port)
      {
         std::lock_guard<Mutex> lock(mgmtdMutex);
         connMgmtdPortUDP = port;
      }

      void resetMgmtdDaemon();

      bool getMailEnabled() const
      {
         return mailEnabled;
      }

      SmtpSendType getMailSmtpSendType() const
      {
         return mailSmtpSendTypeNum;
      }

      const std::string& getMailSendmailPath() const
      {
         return mailSendmailPath;
      }

      const std::string& getMailSmtpServer() const
      {
         return mailSmtpServer;
      }

      const std::string& getMailSender() const
      {
         return mailSender;
      }

      const std::string& getMailRecipient() const
      {
         return mailRecipient;
      }

      int getMailMinDownTimeSec() const
      {
         return mailMinDownTimeSec;
      }

      int getMailResendMailTimeMin() const
      {
         return mailResendMailTimeMin;
      }

      int getMailCheckIntervalTimeSec() const
      {
         return mailCheckIntervalTimeSec;
      }

      bool isMailConfigOverrideActive() const
      {
         return mailEnabled;
      }

};

#endif /*CONFIG_H_*/
