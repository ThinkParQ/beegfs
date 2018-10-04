#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>

class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv);

   private:

      // configurables

      std::string connInterfacesFile;

      unsigned    tuneNumWorkers;

      bool        runDaemonized;

      std::string pidFile;


      // internals

      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;
      std::string createDefaultCfgFilename() const;

   public:
      // getters & setters

      const std::string& getConnInterfacesFile() const
      {
         return connInterfacesFile;
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
};

#endif /*CONFIG_H_*/
