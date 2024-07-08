#include <common/toolkit/StringTk.h>
#include "Config.h"

#define IGNORE_CONFIG_CLIENT_VALUE(keyStr) /* to be used in applyConfigMap() */ \
   if(testConfigMapKeyMatch(iter, keyStr, addDashes) ) \
      ; \
   else

// Note: Keep in sync with enum RunMode
RunModesElem const __RunModes[] =
{
   { "--checkfs", RunMode_CHECKFS },
   { "--enablequota", RunMode_ENABLEQUOTA },
   { NULL, RunMode_INVALID }
};

Config::Config(int argc, char** argv) :
   AbstractConfig(argc, argv)
{
   initConfig(argc, argv, false, true);
   logType = LogType_LOGFILE;
}

/**
 * Determine RunMode from config.
 * If a valid RunMode exists in the config, the corresponding config element will be erased.
 */
enum RunMode Config::determineRunMode()
{
   /* test for given help argument, e.g. in case the user wants to see mode-specific help with
      arguments "--help --<mode>". */

   StringMapIter iter = configMap.find(RUNMODE_HELP_KEY_STRING);
   if(iter != configMap.end() )
   { // user did specify "--help"
      /* note: it's important to remove the help arg here, because mode help will call this again
         to find out whether user wants to see mode-specific help. */
      eraseFromConfigMap(iter);
      return RunMode_HELP;
   }

   // walk all defined modes to check whether we find any of them in the config

   for(int i=0; __RunModes[i].modeString != NULL; i++)
   {
      iter = configMap.find(__RunModes[i].modeString);
      if(iter != configMap.end() )
      { // we found a valid mode in the config
         eraseFromConfigMap(iter);
         return __RunModes[i].runMode;
      }
   }

   // no valid mode found

   return RunMode_INVALID;
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes true to prepend "--" to all config keys.
 */
void Config::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults(addDashes);

   // re-definitions
   configMapRedefine("cfgFile", createDefaultCfgFilename(), addDashes);

   // own definitions
   configMapRedefine("connInterfacesFile", "", addDashes);
   configMapRedefine("connFsckPortUDP", "8005", addDashes);

   configMapRedefine("tuneNumWorkers", "32", addDashes);
   configMapRedefine("tunePreferredNodesFile", "", addDashes);
   configMapRedefine("tuneDbFragmentSize", "0", addDashes);
   configMapRedefine("tuneDentryCacheSize", "0", addDashes);

   configMapRedefine("runDaemonized", "false", addDashes);

   configMapRedefine("databasePath", CONFIG_DEFAULT_DBPATH, addDashes);

   configMapRedefine("overwriteDbFile", "false", addDashes);

   configMapRedefine("testDatabasePath", CONFIG_DEFAULT_TESTDBPATH, addDashes);

   configMapRedefine("DatabaseNumMaxConns", "16", addDashes);

   configMapRedefine("overrideRootMDS", "", addDashes);

   configMapRedefine("logStdFile", CONFIG_DEFAULT_LOGFILE, addDashes);
   configMapRedefine("logOutFile", CONFIG_DEFAULT_OUTFILE, addDashes);
   configMapRedefine("logNoDate", "false", addDashes);

   configMapRedefine("readOnly", "false", addDashes);

   configMapRedefine("noFetch", "false", addDashes);

   configMapRedefine("automatic", "false", addDashes);

   configMapRedefine("runOffline", "false", addDashes);

   configMapRedefine("forceRestart", "false", addDashes);

   configMapRedefine("quotaEnabled", "false", addDashes);

   configMapRedefine("ignoreDBDiskSpace", "false", addDashes);
}

/**
 * @param addDashes true to prepend "--" to tested config keys for matching.
 */
void Config::applyConfigMap(bool enableException, bool addDashes)
{
   AbstractConfig::applyConfigMap(false, addDashes);

   for (StringMapIter iter = configMap.begin(); iter != configMap.end();)
   {
      bool unknownElement = false;

      IGNORE_CONFIG_CLIENT_VALUE("logClientID")
      IGNORE_CONFIG_CLIENT_VALUE("logHelperdIP")
      IGNORE_CONFIG_CLIENT_VALUE("logType")
      IGNORE_CONFIG_CLIENT_VALUE("connNumCommRetries")
      IGNORE_CONFIG_CLIENT_VALUE("connUnmountRetries")
      IGNORE_CONFIG_CLIENT_VALUE("connCommRetrySecs")
      IGNORE_CONFIG_CLIENT_VALUE("connMaxConcurrentAttempts")
      IGNORE_CONFIG_CLIENT_VALUE("connRDMAInterfacesFile")
      IGNORE_CONFIG_CLIENT_VALUE("connTCPFallbackEnabled")
      IGNORE_CONFIG_CLIENT_VALUE("connMessagingTimeouts")
      IGNORE_CONFIG_CLIENT_VALUE("connInterfacesList")
      IGNORE_CONFIG_CLIENT_VALUE("connRDMATimeouts")
      IGNORE_CONFIG_CLIENT_VALUE("connRDMAFragmentSize")
      IGNORE_CONFIG_CLIENT_VALUE("connRDMAKeyType")
      IGNORE_CONFIG_CLIENT_VALUE("connRDMAMetaFragmentSize")
      IGNORE_CONFIG_CLIENT_VALUE("connRDMAMetaBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("connRDMAMetaBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tuneFileCacheType")
      IGNORE_CONFIG_CLIENT_VALUE("tunePagedIOBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tunePagedIOBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneFileCacheBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tuneFileCacheBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("tunePageCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneAttribCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxWriteWorks")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxReadWorks")
      IGNORE_CONFIG_CLIENT_VALUE("tuneAllowMultiSetWrite")
      IGNORE_CONFIG_CLIENT_VALUE("tuneAllowMultiSetRead")
      IGNORE_CONFIG_CLIENT_VALUE("tunePathBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tunePathBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxReadWriteNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxReadWriteNodesNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMsgBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMsgBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneRemoteFSync")
      IGNORE_CONFIG_CLIENT_VALUE("tunePreferredMetaFile")
      IGNORE_CONFIG_CLIENT_VALUE("tunePreferredStorageFile")
      IGNORE_CONFIG_CLIENT_VALUE("tuneUseGlobalFileLocks")
      IGNORE_CONFIG_CLIENT_VALUE("tuneRefreshOnGetAttr")
      IGNORE_CONFIG_CLIENT_VALUE("tuneInodeBlockBits")
      IGNORE_CONFIG_CLIENT_VALUE("tuneInodeBlockSize")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxClientMirrorSize") // was removed, kept here for compat
      IGNORE_CONFIG_CLIENT_VALUE("tuneEarlyCloseResponse")
      IGNORE_CONFIG_CLIENT_VALUE("tuneUseGlobalAppendLocks")
      IGNORE_CONFIG_CLIENT_VALUE("tuneUseBufferedAppend")
      IGNORE_CONFIG_CLIENT_VALUE("tuneStatFsCacheSecs")
      IGNORE_CONFIG_CLIENT_VALUE("sysCreateHardlinksAsSymlinks")
      IGNORE_CONFIG_CLIENT_VALUE("sysMountSanityCheckMS")
      IGNORE_CONFIG_CLIENT_VALUE("sysSyncOnClose")
      IGNORE_CONFIG_CLIENT_VALUE("sysSessionCheckOnClose")
      IGNORE_CONFIG_CLIENT_VALUE("sysSessionChecksEnabled")
      IGNORE_CONFIG_CLIENT_VALUE("sysTargetOfflineTimeoutSecs")
      IGNORE_CONFIG_CLIENT_VALUE("sysInodeIDStyle")
      IGNORE_CONFIG_CLIENT_VALUE("sysACLsEnabled")
      IGNORE_CONFIG_CLIENT_VALUE("sysXAttrsEnabled")
      IGNORE_CONFIG_CLIENT_VALUE("sysXAttrsCheckCapabilities")
      IGNORE_CONFIG_CLIENT_VALUE("tuneDirSubentryCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneFileSubentryCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneENOENTCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneCoherentBuffers")
      IGNORE_CONFIG_CLIENT_VALUE("sysFileEventLogMask")
      IGNORE_CONFIG_CLIENT_VALUE("sysRenameEbusyAsXdev")
      IGNORE_CONFIG_CLIENT_VALUE("tuneNumRetryWorkers")
      IGNORE_CONFIG_CLIENT_VALUE("sysNoEnterpriseFeatureMsg")

      if (testConfigMapKeyMatch(iter, "connInterfacesFile", addDashes))
         connInterfacesFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "connFsckPortUDP", addDashes))
         assignKeyIfNotZero(iter, connFsckPortUDP);
      else if (testConfigMapKeyMatch(iter, "tuneNumWorkers", addDashes))
         tuneNumWorkers = StringTk::strToUInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "tunePreferredNodesFile", addDashes))
         tunePreferredNodesFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "tuneDbFragmentSize", addDashes))
         tuneDbFragmentSize = StringTk::strToUInt64(iter->second.c_str());
      else if (testConfigMapKeyMatch(iter, "tuneDentryCacheSize", addDashes))
         tuneDentryCacheSize = StringTk::strToUInt64(iter->second.c_str());
      else if (testConfigMapKeyMatch(iter, "runDaemonized", addDashes))
         runDaemonized = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "databasePath", addDashes))
         databasePath = iter->second;
      else if (testConfigMapKeyMatch(iter, "overwriteDbFile", addDashes))
         overwriteDbFile = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "testDatabasePath", addDashes))
         testDatabasePath = iter->second;
      else if (testConfigMapKeyMatch(iter, "databaseNumMaxConns", addDashes))
         databaseNumMaxConns = StringTk::strHexToUInt(iter->second);
      else if (testConfigMapKeyMatch(iter, "overrideRootMDS", addDashes))
         overrideRootMDS = iter->second;
      else if (testConfigMapKeyMatch(iter, "logStdFile", addDashes))
         logStdFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "logOutFile", addDashes))
         logOutFile = iter->second;
      else if (testConfigMapKeyMatch(iter, "readOnly", addDashes))
         readOnly = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "noFetch", addDashes))
         noFetch = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "automatic", addDashes))
         automatic = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "runOffline", addDashes))
         runOffline = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "forceRestart", addDashes))
         forceRestart = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "quotaEnabled", addDashes))
         quotaEnabled = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "ignoreDBDiskSpace", addDashes))
         ignoreDBDiskSpace = StringTk::strToBool(iter->second);
      else if (testConfigMapKeyMatch(iter, "checkMalformedChunk", addDashes))
         checkFsActions.set(CHECK_MALFORMED_CHUNK);
      else if (testConfigMapKeyMatch(iter, "checkFilesWithMissingTargets", addDashes))
         checkFsActions.set(CHECK_FILES_WITH_MISSING_TARGETS);
      else if (testConfigMapKeyMatch(iter, "checkOrphanedDentryByIDFiles", addDashes))
         checkFsActions.set(CHECK_ORPHANED_DENTRY_BYIDFILES);
      else if (testConfigMapKeyMatch(iter, "checkDirEntriesWithBrokenIDFile", addDashes))
         checkFsActions.set(CHECK_DIRENTRIES_WITH_BROKENIDFILE);
      else if (testConfigMapKeyMatch(iter, "checkOrphanedChunk", addDashes))
         checkFsActions.set(CHECK_ORPHANED_CHUNK);
      else if (testConfigMapKeyMatch(iter, "checkChunksInWrongPath", addDashes))
         checkFsActions.set(CHECK_CHUNKS_IN_WRONGPATH);
      else if (testConfigMapKeyMatch(iter, "checkWrongInodeOwner", addDashes))
         checkFsActions.set(CHECK_WRONG_INODE_OWNER);
      else if (testConfigMapKeyMatch(iter, "checkWrongOwnerInDentry", addDashes))
         checkFsActions.set(CHECK_WRONG_OWNER_IN_DENTRY);
      else if (testConfigMapKeyMatch(iter, "checkOrphanedContDir", addDashes))
         checkFsActions.set(CHECK_ORPHANED_CONT_DIR);
      else if (testConfigMapKeyMatch(iter, "checkOrphanedDirInode", addDashes))
         checkFsActions.set(CHECK_ORPHANED_DIR_INODE);
      else if (testConfigMapKeyMatch(iter, "checkOrphanedFileInode", addDashes))
         checkFsActions.set(CHECK_ORPHANED_FILE_INODE);
      else if (testConfigMapKeyMatch(iter, "checkDanglingDentry", addDashes))
         checkFsActions.set(CHECK_DANGLING_DENTRY);
      else if (testConfigMapKeyMatch(iter, "checkMissingContDir", addDashes))
         checkFsActions.set(CHECK_MISSING_CONT_DIR);
      else if (testConfigMapKeyMatch(iter, "checkWrongFileAttribs", addDashes))
         checkFsActions.set(CHECK_WRONG_FILE_ATTRIBS);
      else if (testConfigMapKeyMatch(iter, "checkWrongDirAttribs", addDashes))
         checkFsActions.set(CHECK_WRONG_DIR_ATTRIBS);
      else if (testConfigMapKeyMatch(iter, "checkOldStyledHardlinks", addDashes))
         checkFsActions.set(CHECK_OLD_STYLED_HARDLINKS);
      else
      {
         // unknown element occurred
         unknownElement = true;

         if (enableException)
         {
            throw InvalidConfigException("The config argument '" + iter->first + "' is invalid");
         }
      }

      // advance iterator (and remove handled element)

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

void Config::initImplicitVals()
{
   // tuneNumWorkers
   if (!tuneNumWorkers)
      tuneNumWorkers = BEEGFS_MAX(System::getNumOnlineCPUs() * 2, 4);

   if (!tuneDbFragmentSize)
      tuneDbFragmentSize = uint64_t(sysconf(_SC_PHYS_PAGES) ) * sysconf(_SC_PAGESIZE) / 2;

   // just blindly assume that 384 bytes will be enough for a single cache entry. should be
   if (!tuneDentryCacheSize)
      tuneDentryCacheSize = tuneDbFragmentSize / 384;

   // read in connAuthFile only if we are running as root.
   // if not root, the program will abort anyway
   if(!geteuid())
   {
      AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);
   }
}

std::string Config::createDefaultCfgFilename() const
{
   struct stat statBuf;

   const int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);

   if (!statRes && S_ISREG(statBuf.st_mode))
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file

   return ""; // no default file otherwise
}

