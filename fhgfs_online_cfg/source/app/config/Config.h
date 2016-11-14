#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>
#include <modes/Mode.h>

#include <memory>


#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-client.conf"

#define RUNMODE_HELP_KEY_STRING     "--help" /* key for usage help */

struct RunModesElem
{
   const char* modeString;
   bool hidden;

   std::unique_ptr<Mode> (*instantiate)();
   void (*printHelp)();

   bool needsCommunication;
};

extern RunModesElem const __RunModes[];


class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv) throw(InvalidConfigException);

      const RunModesElem* determineRunMode();


   private:

      // configurables
      std::string mount; //special argument to detect configuration file by mount point

      bool        logEnabled;

      std::string connInterfacesFile;

      bool        debugRunComponentThreads;
      bool        debugRunStartupTests;
      bool        debugRunComponentTests;
      bool        debugRunIntegrationTests;
      bool        debugRunThroughputTests;

      unsigned    tuneNumWorkers;
      std::string tunePreferredMetaFile;
      std::string tunePreferredStorageFile;

      bool        runDaemonized;

      // internals

      virtual void loadDefaults(bool addDashes);
      virtual void applyConfigMap(bool enableException, bool addDashes)
         throw(InvalidConfigException);
      virtual void initImplicitVals() throw(InvalidConfigException);
      std::string createDefaultCfgFilename();


   public:
      // getters & setters
      StringMap* getUnknownConfigArgs()
      {
         return getConfigMap();
      }

      std::string getMount() const
      {
         return mount;
      }

      bool getLogEnabled() const
      {
         return logEnabled;
      }

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

      bool getDebugRunThroughputTests() const
      {
         return debugRunThroughputTests;
      }

      unsigned getTuneNumWorkers() const
      {
         return tuneNumWorkers;
      }

      std::string getTunePreferredMetaFile() const
      {
         return tunePreferredMetaFile;
      }

      std::string getTunePreferredStorageFile() const
      {
         return tunePreferredStorageFile;
      }

      bool getRunDaemonized() const
      {
         return runDaemonized;
      }
};

#endif /*CONFIG_H_*/
