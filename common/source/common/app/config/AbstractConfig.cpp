#include <common/toolkit/StringTk.h>
#include <common/toolkit/StorageTk.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include "AbstractConfig.h"
#include "common/toolkit/HashTk.h"


#define ABSTRACTCONF_AUTHFILE_READSIZE 1024 // max amount of data that we read from auth file
#define ABSTRACTCONF_AUTHFILE_MINSIZE  4 // at least 2, because we compute two 32bit hashes


AbstractConfig::AbstractConfig(int argc, char** argv) :
      argc(argc), argv(argv)
{
   // initConfig(..) must be called by derived classes
   // because of the virtual init method loadDefaults()
}

/**
 * Note: This must not be called from the AbstractConfig (or base class) constructor, because it
 * calls virtual methods of the sub classes.
 *
 * @param enableException: true to throw an exception when an unknown config element is found,
 * false to ignore it.
 * @param addDashes true to prepend dashes to defaults and config file keys (used e.g. by fhgfs-ctl)
 */
void AbstractConfig::initConfig(int argc, char** argv, bool enableException, bool addDashes)
{
   // load and apply args to see whether we have a cfgFile
   loadDefaults(addDashes);
   loadFromArgs(argc, argv);

   applyConfigMap(enableException, addDashes);

   if(this->cfgFile.length() )
   { // there is a config file specified
      // start over again and include the config file this time
      this->configMap.clear();

      const std::string tmpCfgFile = this->cfgFile; /* need tmp variable here to make sure we don't
         override the value when we use call loadDefaults() again below, because subclasses have
         direct access to this->cfgFile */

      loadDefaults(addDashes);
      loadFromFile(tmpCfgFile.c_str(), addDashes);
      loadFromArgs(argc, argv);

      applyConfigMap(enableException, addDashes);
   }

   initImplicitVals();
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes true to prepend "--" to all config keys.
 */
void AbstractConfig::loadDefaults(bool addDashes)
{
   configMapRedefine("cfgFile",                  "", addDashes);

   configMapRedefine("logType",                  "logfile", addDashes);
   configMapRedefine("logLevel",                 "3", addDashes);
   configMapRedefine("logNoDate",                "true", addDashes);
   configMapRedefine("logStdFile",               "", addDashes);
   configMapRedefine("logNumLines",              "50000", addDashes);
   configMapRedefine("logNumRotatedFiles",       "2", addDashes);

   configMapRedefine("connPortShift",              "0", addDashes);

   // To be able to merge these with the legacy settings later, we set them to -1 here. Otherwise it
   // is impossible to detect if they have actually been set or just loaded the default.
   // The actual default values are applied during the post processing in applyConfigMap.
   configMapRedefine("connClientPort",             "-1", addDashes); // 8004
   configMapRedefine("connStoragePort",            "-1", addDashes); // 8003
   configMapRedefine("connMetaPort",               "-1", addDashes); // 8005
   configMapRedefine("connMonPort",                "-1", addDashes); // 8007
   configMapRedefine("connMgmtdPort",              "-1", addDashes); // 8008

   configMapRedefine("connUseRDMA",                "true", addDashes);
   configMapRedefine("connBacklogTCP",             "64", addDashes);
   configMapRedefine("connMaxInternodeNum",        "6", addDashes);
   configMapRedefine("connFallbackExpirationSecs", "900", addDashes);
   configMapRedefine("connTCPRcvBufSize",          "0", addDashes);
   configMapRedefine("connUDPRcvBufSize",          "0", addDashes);
   configMapRedefine("connRDMABufSize",            "8192", addDashes);
   configMapRedefine("connRDMABufNum",             "70", addDashes);
   configMapRedefine("connRDMATypeOfService",      "0", addDashes);
   configMapRedefine("connNetFilterFile",          "", addDashes);
   configMapRedefine("connAuthFile",               "", addDashes);
   configMapRedefine("connDisableAuthentication",  "false", addDashes);
   configMapRedefine("connTcpOnlyFilterFile",      "", addDashes);
   configMapRedefine("connRestrictOutboundInterfaces", "false", addDashes);
   configMapRedefine("connNoDefaultRoute",         "0.0.0.0/0", addDashes);

   /* connMessagingTimeouts: default to zero, indicating that constants
    * specified in Common.h are used.
    */
   configMapRedefine("connMessagingTimeouts",      "0,0,0", addDashes);
   // connRDMATimeouts: zero values are interpreted as the defaults specified in IBVSocket.cpp
   configMapRedefine("connRDMATimeouts",           "0,0,0", addDashes);

   configMapRedefine("sysMgmtdHost",               "",    addDashes);
   configMapRedefine("sysUpdateTargetStatesSecs",  "0", addDashes);
}

/**
 * @param enableException: true to throw an exception when an unknown config element is found,
 * false to ignore it.
 * @param addDashes true to prepend "--" to tested config keys for matching.
 */
void AbstractConfig::applyConfigMap(bool enableException, bool addDashes)
{
   // Deprecated separate port settings. These are post processed below and merged into the new
   // combined settings.
   int connClientPortUDP = -1;
   int connMgmtdPortUDP = -1;
   int connMetaPortUDP = -1;
   int connStoragePortUDP = -1;
   int connMonPortUDP = -1;
   int connClientPortTCP = -1;
   int connMgmtdPortTCP = -1;
   int connMetaPortTCP = -1;
   int connStoragePortTCP = -1;
   int connMonPortTCP = -1;

   for (StringMapIter iter = configMap.begin(); iter != configMap.end();)
   {
      bool unknownElement = false;

      if (testConfigMapKeyMatch(iter, "cfgFile", addDashes))
         cfgFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "logLevel", addDashes))
         logLevel = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "logNoDate", addDashes))
         logNoDate = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "logStdFile", addDashes))
         logStdFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "logNumLines", addDashes))
         logNumLines = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "logNumRotatedFiles", addDashes))
         logNumRotatedFiles = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connPortShift", addDashes))
         connPortShift = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connClientPort", addDashes))
         assignKeyIfNotZero(iter, connClientPort, enableException);
      else if (testConfigMapKeyMatch(iter, "connStoragePort", addDashes))
         assignKeyIfNotZero(iter, connStoragePort, enableException);
      else if (testConfigMapKeyMatch(iter, "connMetaPort", addDashes))
         assignKeyIfNotZero(iter, connMetaPort, enableException);
      else if (testConfigMapKeyMatch(iter, "connMonPort", addDashes))
         assignKeyIfNotZero(iter, connMonPort, enableException);
      else if (testConfigMapKeyMatch(iter, "connMgmtdPort", addDashes))
         assignKeyIfNotZero(iter, connMgmtdPort, enableException);
      else if (testConfigMapKeyMatch(iter, "connClientPortUDP", addDashes))
         assignKeyIfNotZero(iter, connClientPortUDP, enableException);
      else if (testConfigMapKeyMatch(iter, "connStoragePortUDP", addDashes))
         assignKeyIfNotZero(iter, connStoragePortUDP, enableException);
      else if (testConfigMapKeyMatch(iter, "connMetaPortUDP", addDashes))
         assignKeyIfNotZero(iter, connMetaPortUDP, enableException);
      else if (testConfigMapKeyMatch(iter, "connMonPortUDP", addDashes))
         assignKeyIfNotZero(iter, connMonPortUDP, enableException);
      else if (testConfigMapKeyMatch(iter, "connMgmtdPortUDP", addDashes))
         assignKeyIfNotZero(iter, connMgmtdPortUDP, enableException);
      else if (testConfigMapKeyMatch(iter, "connStoragePortTCP", addDashes))
         assignKeyIfNotZero(iter, connStoragePortTCP, enableException);
      else if (testConfigMapKeyMatch(iter, "connMetaPortTCP", addDashes))
         assignKeyIfNotZero(iter, connMetaPortTCP, enableException);
      else if (testConfigMapKeyMatch(iter, "connMgmtdPortTCP", addDashes))
         assignKeyIfNotZero(iter, connMgmtdPortTCP, enableException);
      else if (testConfigMapKeyMatch(iter, "connUseRDMA", addDashes))
         connUseRDMA = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "connBacklogTCP", addDashes))
         connBacklogTCP = StringTk::strToUInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connMaxInternodeNum", addDashes))
         connMaxInternodeNum = StringTk::strToUInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connNonPrimaryExpiration", addDashes))
      {
         // superseded by connFallbackExpirationSecs, ignored here for config file compatibility
      }
      else if (testConfigMapKeyMatch(iter, "connFallbackExpirationSecs", addDashes))
         connFallbackExpirationSecs = StringTk::strToUInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connTCPRcvBufSize", addDashes))
         connTCPRcvBufSize = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connUDPRcvBufSize", addDashes))
         connUDPRcvBufSize = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connRDMABufSize", addDashes))
         connRDMABufSize = StringTk::strToUInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connRDMABufNum", addDashes))
         connRDMABufNum = StringTk::strToUInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connRDMATypeOfService", addDashes))
         connRDMATypeOfService = (uint8_t)StringTk::strToUInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connNetFilterFile", addDashes))
         connNetFilterFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "connAuthFile", addDashes))
         connAuthFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "connDisableAuthentication", addDashes))
         connDisableAuthentication = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "connTcpOnlyFilterFile", addDashes))
         connTcpOnlyFilterFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "connRestrictOutboundInterfaces", addDashes))
         connRestrictOutboundInterfaces = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "connNoDefaultRoute", addDashes))
         connNoDefaultRoute = iter->second;
      else if (testConfigMapKeyMatch(iter, "connMessagingTimeouts", addDashes))
      {
         const size_t cfgValCount = 3; // count value in config file in order of long, medium and short
         std::list<std::string> split;
         StringList::iterator timeoutIter;

         StringTk::explode(iter->second, ',', &split);

         if (split.size() == cfgValCount)
         {
            timeoutIter = split.begin();
            connMsgLongTimeout = StringTk::strToInt(*timeoutIter) > 0 ?
                  StringTk::strToInt(*timeoutIter) : CONN_LONG_TIMEOUT;
            timeoutIter++;

            connMsgMediumTimeout = StringTk::strToInt(*timeoutIter) > 0 ?
                  StringTk::strToInt(*timeoutIter) : CONN_MEDIUM_TIMEOUT;
            timeoutIter++;

            connMsgShortTimeout = StringTk::strToInt(*timeoutIter) > 0 ?
                  StringTk::strToInt(*timeoutIter) : CONN_SHORT_TIMEOUT;
         }
         else
            throw InvalidConfigException("The config argument '" + iter->first + "' is invalid");
      }
      else if (testConfigMapKeyMatch(iter, "connRDMATimeouts", addDashes))
      {
         const size_t cfgValCount = 3;
         const size_t cfgUtilValCount = 5;
         std::list<std::string> split;

         StringTk::explode(iter->second, ',', &split);
         // YUCK. beegfs-ctl and beegfs-fsck use beegfs-client.conf. That file defines
         // connRDMATimeouts as 5 comma-separated values, but user space only has 3
         // values. The utils are supposed to ignore the configuration. cfgUtilValCount
         // is a hack to prevent the configuration from causing a runtime error.
         if (split.size() != cfgUtilValCount)
         {
            if (split.size() == cfgValCount)
            {
               StringList::iterator timeoutIter = split.begin();
               connRDMATimeoutConnect = StringTk::strToInt(*timeoutIter++);
               connRDMATimeoutFlowSend = StringTk::strToInt(*timeoutIter++);
               connRDMATimeoutPoll = StringTk::strToInt(*timeoutIter);
            }
            else
            {
               throw InvalidConfigException("The config argument '" + iter->first + "' is invalid");
            }
         }
      }
      else if (testConfigMapKeyMatch(iter, "sysMgmtdHost", addDashes))
         sysMgmtdHost = iter->second;
      else if (testConfigMapKeyMatch(iter, "sysUpdateTargetStatesSecs", addDashes))
         sysUpdateTargetStatesSecs = StringTk::strToUInt(iter->second);
      else
      {
         // unknown element occurred
         unknownElement = true;

         if (enableException)
         {
            throw InvalidConfigException("The config argument '" + iter->first + "' is invalid");
         }
      }

      if (unknownElement)
      {
         // just skip the unknown element
         iter++;
      }
      else
      {
         // remove this element from the map
         iter = eraseFromConfigMap(iter);
      }
   }

   auto processPortSettings = [&](const std::string& name, int& setting, const int& tcp, const int& udp, const int def) {
      if(setting == -1) {
         if(tcp != -1 && udp != -1 && tcp != udp) {
            throw InvalidConfigException("Deprecated config arguments '" + name + "UDP' and \
'" + name + "TCP' set to different values, which is no longer allowed. Please use the new '" + name + "' \
setting instead.");
         }

         // Set the new setting using the old values
         if(tcp != -1) {
            setting = tcp;
            // TODO Doesn't work as the logger isn't initialized yet: https://github.com/ThinkParQ/beegfs-core/issues/4033
            LOG(GENERAL, WARNING, "Using deprecated config argument '" + name + "TCP'");
         } else if(udp != -1) {
            setting = udp;
            // TODO Doesn't work as the logger isn't initialized yet: https://github.com/ThinkParQ/beegfs-core/issues/4033
            LOG(GENERAL, WARNING, "Using deprecated config argument '" + name + "UDP'");
         } else {
            setting = def;
         }
      } else {
         if(tcp != -1 || udp != -1) {
            throw InvalidConfigException("Deprecated config arguments '" + name + "UDP/TCP' set along with the new \
'" + name + "' setting. Please use only the new setting.");
         }
      }
   };

   processPortSettings("connClientPort", this->connClientPort, connClientPortTCP, connClientPortUDP, 8004);
   processPortSettings("connStoragePort", this->connStoragePort, connStoragePortTCP, connStoragePortUDP, 8003);
   processPortSettings("connMetaPort", this->connMetaPort, connMetaPortTCP, connMetaPortUDP, 8005);
   processPortSettings("connMonPort", this->connMonPort, connMonPortTCP, connMonPortUDP, 8007);
   processPortSettings("connMgmtdPort", this->connMgmtdPort, connMgmtdPortTCP, connMgmtdPortUDP, 8008);
}

void AbstractConfig::initImplicitVals()
{
   // nothing to be done here (just a dummy so that derived classes don't have to impl this)
}

/**
 * Initialize interfaces list from interfaces file if the list is currently empty and the filename
 * is not empty.
 *
 * @param inoutConnInterfacesList will be initialized from file (comma-separated) if it was empty
 *
 * @throw InvalidConfigException if interfaces filename and interface list are both not empty
 */
void AbstractConfig::initInterfacesList(const std::string& connInterfacesFile,
                                        std::string& inoutConnInterfacesList)
{
   // make sure not both (list and file) were specified
   if(!inoutConnInterfacesList.empty() && !connInterfacesFile.empty() )
   {
      throw InvalidConfigException(
         "connInterfacesFile and connInterfacesList cannot be used together");
   }
   
   if(!inoutConnInterfacesList.empty() )
      return; // interfaces already given as list => nothing to do

   if(connInterfacesFile.empty() )
      return; // no interfaces file given => nothing to do


   // load interfaces from file...

   StringList loadedInterfacesList;

   loadStringListFile(connInterfacesFile.c_str(), loadedInterfacesList);

   inoutConnInterfacesList = StringTk::implode(',', loadedInterfacesList, true);
}

/**
 * Generate connection authentication hash based on contents of given authentication file.
 *
 * @param outConnAuthHash will be set to 0 if file is not defined
 *
 * @throw InvalidConfigException if connAuthFile is defined, but cannot be read.
 */
void AbstractConfig::initConnAuthHash(const std::string& connAuthFile, uint64_t* outConnAuthHash)
{
   if (connDisableAuthentication) {
      *outConnAuthHash = 0;
      return; // connAuthFile explicitly disabled => no hash to be generated
   }

   // Connection authentication not explicitly disabled, so bail if connAuthFile is not configured
   if(connAuthFile.empty())
      throw ConnAuthFileException("No connAuthFile configured. Using BeeGFS without connection authentication is considered insecure and is not recommended. If you really want or need to run BeeGFS without connection authentication, please set connDisableAuthentication to true.");

   // open file...

   /* note: we don't reuse something like loadStringListFile() here, because:
       1) the auth file might not contain a string, but can be any binary data (including zeros).
       2) we want to react on EACCES. */

   int fd = open(connAuthFile.c_str(), O_RDONLY);

   int errCode = errno;

   if( (fd == -1) && (errCode == EACCES) )
   { // current effective user/group ID not allowed to read file => try it with saved IDs
      unsigned previousUID;
      unsigned previousGID;

      // switch to saved IDs
      System::elevateUserAndGroupFsID(&previousUID, &previousGID);

      fd = open(connAuthFile.c_str(), O_RDONLY);

      errCode = errno; // because ID dropping might change errno

      // restore previous IDs
      System::setFsIDs(previousUID, previousGID, &previousUID, &previousGID);
   }

   if(fd == -1)
   {
      throw ConnAuthFileException("Unable to open auth file: " + connAuthFile + " "
            "(SysErr: " + System::getErrString(errCode) + ")");
   }

   // load file contents...

   unsigned char buf[ABSTRACTCONF_AUTHFILE_READSIZE];

   const ssize_t readRes = read(fd, buf, ABSTRACTCONF_AUTHFILE_READSIZE);

   errCode = errno; // because close() might change errno

   close(fd);

   if(readRes < 0)
   {
      throw InvalidConfigException("Unable to read auth file: " + connAuthFile + " "
         "(SysErr: " + System::getErrString(errCode) + ")");
   }

   // empty authFile is probably unintended, so treat it as error
   if(!readRes || (readRes < ABSTRACTCONF_AUTHFILE_MINSIZE) )
      throw InvalidConfigException("Auth file is empty: " + connAuthFile);


   // hash file contents
   *outConnAuthHash = HashTk::authHash(buf, readRes);
}

/**
 * Sets the value of connTCPRcvBufSize and connUDPRcvBufSize according to the configuration.
 * 0 indicates legacy behavior that uses RDMA bufsizes. Otherwise leave the values as
 * configured.
 */
void AbstractConfig::initSocketBufferSizes()
{
   int legacy = connRDMABufSize * connRDMABufNum;

   if (connTCPRcvBufSize == 0)
      connTCPRcvBufSize = legacy;

   if (connUDPRcvBufSize == 0)
      connUDPRcvBufSize = legacy;
}

/**
 * Removes an element from the config map and returns an iterator that is positioned right
 * after the removed element
 */
StringMapIter AbstractConfig::eraseFromConfigMap(StringMapIter iter)
{
   StringMapIter nextIter(iter);

   nextIter++; // save position after the erase element

   configMap.erase(iter);

   return nextIter;
}

/**
 * @param addDashes true to prepend "--" to every config key.
 */
void AbstractConfig::loadFromFile(const char* filename, bool addDashes)
{
   if(!addDashes)
   { // no dashes needed => load directly into configMap
      MapTk::loadStringMapFromFile(filename, &configMap);
      return;
   }

   /* we need to add dashes to keys => use temporary map with real keys and then copy to actual
      config map with prepended dashes. */

   StringMap tmpMap;

   MapTk::loadStringMapFromFile(filename, &tmpMap);

   for(StringMapCIter iter = tmpMap.begin(); iter != tmpMap.end(); iter++)
      configMapRedefine(iter->first, iter->second, addDashes);
}

/**
 * Note: No addDashes param here, because the user should specify dashes himself on the command line
 * if they are needed.
 */
void AbstractConfig::loadFromArgs(int argc, char** argv)
{
   for(int i=1; i < argc; i++)
      MapTk::addLineToStringMap(argv[i], &configMap);
}


/**
 * @warning It might exit the proccess if it finds an incorrect value
 */
void AbstractConfig::assignKeyIfNotZero(const StringMapIter& it, int& intVal, bool enableException)
{
   const int tempVal = StringTk::strToInt(it->second);

   if (tempVal == 0) {
      if (enableException) {
         throw InvalidConfigException("Invalid or unset configuration variable: " + (it->first));
      }
      return;
   }

   intVal = tempVal;
}

