#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>

class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv) throw(InvalidConfigException);
   
   private:

      // configurables

      std::string connInterfacesFile;

      bool        debugRunComponentThreads;
      bool        debugRunStartupTests;
      bool        debugRunComponentTests;
      bool        debugRunIntegrationTests;
      
      unsigned    tuneNumWorkers;
      
      bool        runDaemonized;

      std::string pidFile;
      

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
};

#endif /*CONFIG_H_*/
