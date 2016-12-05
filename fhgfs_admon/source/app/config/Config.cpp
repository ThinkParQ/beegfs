#include <common/toolkit/StringTk.h>
#include <program/Program.h>
#include "Config.h"

#include <sys/stat.h>



#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-admon.conf"

#define SMTPSENDTYPE_SOCKET        "socket"
#define SMTPSENDTYPE_SENDMAIL      "sendmail"



Config::Config(int argc, char** argv) throw(InvalidConfigException) : AbstractConfig(argc, argv)
{
   initConfig(argc, argv, true);
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes currently unused
 */
void Config::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults();

   // re-definitions
   configMapRedefine("cfgFile",                  "");
   configMapRedefine("connUseRDMA",              "false"); // overrides abstract config

   // own definitions
   configMapRedefine("connInterfacesFile",       "");

   configMapRedefine("debugRunComponentThreads", "true");
   configMapRedefine("debugRunStartupTests",     "false");
   configMapRedefine("debugRunComponentTests",   "false");
   configMapRedefine("debugRunIntegrationTests", "false");

   configMapRedefine("tuneNumWorkers",           "4");

   configMapRedefine("runDaemonized",            "false");

   configMapRedefine("pidFile",                  "");

   configMapRedefine("httpPort",                 "8000");
   configMapRedefine("queryInterval",            "10");
   configMapRedefine("reloadInterval",           "10");
   configMapRedefine("databaseFile",             "/var/lib/beegfs/beegfs-admon.db");
   configMapRedefine("clearDatabase",            "false");

   configMapRedefine("mailEnabled",              "false");
   configMapRedefine("mailSmtpSendType",         SMTPSENDTYPE_SOCKET);
   configMapRedefine("mailSendmailPath",         MAIL_SENDMAIL_DEFAULT_PATH);
   configMapRedefine("mailSmtpServer",           "");
   configMapRedefine("mailSender",               "");
   configMapRedefine("mailRecipient",            "");
   configMapRedefine("mailMinDownTimeSec",       "10");
   configMapRedefine("mailResendMailTimeMin",    "60");
   configMapRedefine("mailCheckIntervalTimeSec", "30");
}

/**
 * @param addDashes currently usused
 */
void Config::applyConfigMap(bool enableException, bool addDashes) throw(InvalidConfigException)
{
   AbstractConfig::applyConfigMap(false);

   for(StringMapIter iter = getConfigMap()->begin(); iter != getConfigMap()->end(); )
   {
      bool unknownElement = false;

      if(iter->first == std::string("connInterfacesFile") )
         connInterfacesFile = iter->second;
      else
      if(iter->first == std::string("debugRunComponentThreads") )
         debugRunComponentThreads = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("debugRunStartupTests") )
         debugRunStartupTests = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("debugRunComponentTests") )
         debugRunComponentTests = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("debugRunIntegrationTests") )
         debugRunIntegrationTests = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneNumWorkers") )
         tuneNumWorkers = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("runDaemonized") )
         runDaemonized = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("pidFile") )
         pidFile = iter->second;
      else
      if(iter->first == std::string("httpPort") )
         httpPort = StringTk::strToUInt(iter->second);
      else
      if (iter->first == std::string("queryInterval"))
         queryInterval = StringTk::strToInt(iter->second);
      else
      if (iter->first == std::string("reloadInterval"))
         reloadInterval = StringTk::strToInt(iter->second);
      else
      if(iter->first == std::string("databaseFile") )
         databaseFile = iter->second;
      else
      if(iter->first == std::string("clearDatabase") )
         clearDatabase = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("mailEnabled") )
         mailEnabled = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("mailSmtpSendType") )
         mailSmtpSendType = iter->second;
      else
      if(iter->first == std::string("mailSendmailPath") )
         mailSendmailPath = iter->second;
      else
      if(iter->first == std::string("mailSmtpServer") )
         mailSmtpServer = iter->second;
      else
      if(iter->first == std::string("mailSender") )
         mailSender = iter->second;
      else
      if(iter->first == std::string("mailRecipient") )
         mailRecipient = iter->second;
      else
      if(iter->first == std::string("mailMinDownTimeSec") )
         mailMinDownTimeSec = StringTk::strToInt(iter->second);
      else
         if(iter->first == std::string("mailResendMailTimeMin") )
         mailResendMailTimeMin = StringTk::strToInt(iter->second);
      else
      if(iter->first == std::string("mailCheckIntervalTimeSec") )
         mailCheckIntervalTimeSec = StringTk::strToInt(iter->second);
      else
      { // unknown element occurred
         unknownElement = true;

         if(enableException)
         {
            throw InvalidConfigException(
               std::string("The config argument '") + iter->first + std::string("' is invalid") );
         }
      }

      if(unknownElement)
      { // just skip the unknown element
         iter++;
      }
      else
      { // remove this element from the map
         iter = eraseFromConfigMap(iter);
      }
   }
}

void Config::initImplicitVals() throw(InvalidConfigException)
{
   // connAuthHash
   AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);

   initMailerConfig();
}

std::string Config::createDefaultCfgFilename()
{
   struct stat statBuf;

   int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);

   if(!statRes && S_ISREG(statBuf.st_mode) )
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file

   return ""; // no default file otherwise
}

void Config::resetMgmtdDaemon()
{
   StringMap cfgMap;

   SafeMutexLock mutexLock(&mgmtdMutex);

   MapTk::loadStringMapFromFile(getCfgFile().c_str(), &cfgMap);

   for (StringMapIter iter = cfgMap.begin(); iter != cfgMap.end(); iter++)
   {
      if (iter->first == std::string("connMgmtdPortUDP"))
         connMgmtdPortUDP = StringTk::strToInt(iter->second);
      else
      if (iter->first == std::string("connMgmtdPortTCP"))
         connMgmtdPortTCP = StringTk::strToInt(iter->second);
      else
      if (iter->first == std::string("sysMgmtdHost"))
         sysMgmtdHost = iter->second;
   }

   Program::getApp()->getMgmtNodes()->deleteAllNodes();

   mutexLock.unlock();
}

/**
 * Converts the given parameter mailSmtpSendType into a enum value SmtpSendType_... and checks if
 * the sendmail path is valid.
 *
 * @throws InvalidConfigException If the mailSmtpSendType is invalid or the sendmail path is not
 *         valid.
 */
void Config::initMailerConfig()
{
   if(this->mailSmtpSendType == SMTPSENDTYPE_SOCKET)
   {
      this->mailSmtpSendTypeNum = SmtpSendType_SOCKET;
   }
   else
   if(this->mailSmtpSendType == SMTPSENDTYPE_SENDMAIL)
   {
      this->mailSmtpSendTypeNum = SmtpSendType_SENDMAIL;

      // the default must be overridden by an empty string
      if(mailSendmailPath.empty() )
         throw InvalidConfigException("Path to sendmail path is empty.");

      // a path to the sendmail binary was given which is not the default path
      if(mailSendmailPath != MAIL_SENDMAIL_DEFAULT_PATH)
      {
         struct stat statBuffer;
         int statRetVal = stat(mailSendmailPath.c_str(), &statBuffer);

         if(statRetVal)
            throw InvalidConfigException("Stat of given sendmail path failed: " + mailSendmailPath +
               " ; Errno: " + System::getErrString() );
      }
   }
   else
   { // invalid chooser specified
      throw InvalidConfigException("Invalid SMTP send type specified: " + mailSmtpSendType);
   }
}
