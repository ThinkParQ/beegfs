#include <common/toolkit/StringTk.h>
#include <common/toolkit/StorageTk.h>
#include "AbstractConfig.h"


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
   configMapRedefine("connClientPortUDP",          "8004", addDashes);
   configMapRedefine("connStoragePortUDP",         "8003", addDashes);
   configMapRedefine("connMetaPortUDP",            "8005", addDashes);
   configMapRedefine("connAdmonPortUDP",           "8007", addDashes);
   configMapRedefine("connMgmtdPortUDP",           "8008", addDashes);
   configMapRedefine("connStoragePortTCP",         "8003", addDashes);
   configMapRedefine("connMetaPortTCP",            "8005", addDashes);
   configMapRedefine("connHelperdPortTCP",         "8006", addDashes);
   configMapRedefine("connMgmtdPortTCP",           "8008", addDashes);
   configMapRedefine("connUseRDMA",                "true", addDashes);
   configMapRedefine("connBacklogTCP",             "64", addDashes);
   configMapRedefine("connMaxInternodeNum",        "6", addDashes);
   configMapRedefine("connFallbackExpirationSecs", "900", addDashes);
   configMapRedefine("connRDMABufSize",            "8192", addDashes);
   configMapRedefine("connRDMABufNum",             "70", addDashes);
   configMapRedefine("connRDMATypeOfService",      "0", addDashes);
   configMapRedefine("connNetFilterFile",          "", addDashes);
   configMapRedefine("connAuthFile",               "", addDashes);
   configMapRedefine("connTcpOnlyFilterFile",      "", addDashes);

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
      else if (testConfigMapKeyMatch(iter, "connClientPortUDP", addDashes))
         connClientPortUDP = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connStoragePortUDP", addDashes))
         connStoragePortUDP = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connMetaPortUDP", addDashes))
         connMetaPortUDP = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connAdmonPortUDP", addDashes))
         connAdmonPortUDP = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connMgmtdPortUDP", addDashes))
         connMgmtdPortUDP = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connStoragePortTCP", addDashes))
         connStoragePortTCP = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connMetaPortTCP", addDashes))
         connMetaPortTCP = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connHelperdPortTCP", addDashes))
         connHelperdPortTCP = StringTk::strToInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "connMgmtdPortTCP", addDashes))
         connMgmtdPortTCP = StringTk::strToInt(iter->second);
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
      else if (testConfigMapKeyMatch(iter, "connTcpOnlyFilterFile", addDashes))
         connTcpOnlyFilterFile = iter->second;
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
   if(connAuthFile.empty() )
   {
      *outConnAuthHash = 0;
      return; // no file given => no hash to be generated
   }

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
      throw InvalidConfigException("Unable to open auth file: " + connAuthFile + " "
            "(SysErr: " + System::getErrString(errCode) + ")");
   }

   // load file contents...

   char buf[ABSTRACTCONF_AUTHFILE_READSIZE];

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
   // (hsieh hash only generates 32bit hashes, so we make two hashes for 64 bits)

   int len1stHalf = readRes / 2;
   int len2ndHalf = readRes - len1stHalf;

   const uint64_t high = BufferTk::hash32(buf, len1stHalf);
   const uint64_t low  = BufferTk::hash32(&buf[len1stHalf], len2ndHalf);

   *outConnAuthHash = (high << 32) | low;
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

