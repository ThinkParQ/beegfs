#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>
#include <bitset>

#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-client.conf"
#define CONFIG_DEFAULT_LOGFILE     "/var/log/beegfs-fsck.log"
#define CONFIG_DEFAULT_OUTFILE     "/var/log/beegfs-fsck.out"
#define CONFIG_DEFAULT_DBPATH      "/var/lib/beegfs/"
#define CONFIG_DEFAULT_TESTDBPATH  "/tmp/beegfs-fsck/"

#define RUNMODE_HELP_KEY_STRING     "--help" /* key for usage help */
#define __RUNMODES_SIZE \
   ( (sizeof(__RunModes) ) / (sizeof(RunModesElem) ) - 1)
   /* -1 because last elem is NULL */

enum CheckFsActions
{
   CHECK_MALFORMED_CHUNK               = 0,
   CHECK_FILES_WITH_MISSING_TARGETS    = 1,
   CHECK_ORPHANED_DENTRY_BYIDFILES     = 2,
   CHECK_DIRENTRIES_WITH_BROKENIDFILE  = 3,
   CHECK_ORPHANED_CHUNK                = 4,
   CHECK_CHUNKS_IN_WRONGPATH           = 5,
   CHECK_WRONG_INODE_OWNER             = 6,
   CHECK_WRONG_OWNER_IN_DENTRY         = 7,
   CHECK_ORPHANED_CONT_DIR             = 8,
   CHECK_ORPHANED_DIR_INODE            = 9,
   CHECK_ORPHANED_FILE_INODE           = 10,
   CHECK_DANGLING_DENTRY               = 11,
   CHECK_MISSING_CONT_DIR              = 12,
   CHECK_WRONG_FILE_ATTRIBS            = 13,
   CHECK_WRONG_DIR_ATTRIBS             = 14,
   CHECK_OLD_STYLED_HARDLINKS          = 15,
   CHECK_FS_ACTIONS_COUNT              = 16
};

// Note: Keep in sync with __RunModes array
enum RunMode
{
   RunMode_CHECKFS              =     0,
   RunMode_ENABLEQUOTA          =     1,
   RunMode_HELP                 =     2,
   RunMode_INVALID              =     3 /* not valid as index in RunModes array */
};

struct RunModesElem
{
   const char* modeString;
   enum RunMode runMode;
};


extern RunModesElem const __RunModes[];

class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv);

      enum RunMode determineRunMode();

   private:

      // configurables

      std::string connInterfacesFile;
      int         connFsckPortUDP;

      unsigned    tuneNumWorkers;
      std::string tunePreferredNodesFile;
      size_t      tuneDbFragmentSize;
      size_t      tuneDentryCacheSize;

      bool        runDaemonized;

      std::string databasePath;
      bool        overwriteDbFile;

      // only relevant for unit testing, to give the used databasePath
      std::string testDatabasePath;

      unsigned    databaseNumMaxConns;

      std::string overrideRootMDS; // not tested well, should only be used by developers

      // file for fsck output (not the log messages, but the output, which is also on the console)
      std::string logOutFile;

      bool        readOnly;
      bool        noFetch;
      bool        automatic;
      bool        runOffline;
      bool        forceRestart;
      bool        quotaEnabled;
      bool        ignoreDBDiskSpace;

      std::bitset<CHECK_FS_ACTIONS_COUNT> checkFsActions;

      // internals
      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;
      std::string createDefaultCfgFilename() const;

   public:
      // getters & setters
      const StringMap* getUnknownConfigArgs() const
      {
         return getConfigMap();
      }

      const std::string& getConnInterfacesFile() const
      {
         return connInterfacesFile;
      }

      int getConnFsckPortUDP() const
      {
         return connFsckPortUDP;
      }

      unsigned getTuneNumWorkers() const
      {
         return tuneNumWorkers;
      }

      size_t getTuneDbFragmentSize() const
      {
         return tuneDbFragmentSize;
      }

      size_t getTuneDentryCacheSize() const
      {
         return tuneDentryCacheSize;
      }

      const std::string& getTunePreferredNodesFile() const
      {
         return tunePreferredNodesFile;
      }

      bool getRunDaemonized() const
      {
         return runDaemonized;
      }

      const std::string& getDatabasePath() const
      {
         return databasePath;
      }

      bool getOverwriteDbFile() const
      {
         return overwriteDbFile;
      }

      const std::string& getTestDatabasePath() const
      {
         return testDatabasePath;
      }

      unsigned getDatabaseNumMaxConns() const
      {
         return databaseNumMaxConns;
      }

      std::string getOverrideRootMDS() const
      {
         return overrideRootMDS;
      }

      bool getReadOnly() const
      {
         return readOnly;
      }

      bool getNoFetch() const
      {
         return noFetch;
      }

      bool getAutomatic() const
      {
         return automatic;
      }

      const std::string& getLogOutFile() const
      {
         return logOutFile;
      }

      bool getRunOffline() const
      {
         return runOffline;
      }

      bool getForceRestart() const
      {
         return forceRestart;
      }

      bool getQuotaEnabled() const
      {
         return quotaEnabled;
      }

      bool getIgnoreDBDiskSpace() const
      {
         return ignoreDBDiskSpace;
      }

      std::bitset<CHECK_FS_ACTIONS_COUNT> getCheckFsActions() const
      {
         return checkFsActions;
      }

      void disableAutomaticRepairMode()
      {
         this->automatic = false;
      }

      void setReadOnly()
      {
         this->readOnly = true;
      }
};

#endif /*CONFIG_H_*/
