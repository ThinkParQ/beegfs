#ifndef CONFIG_H_
#define CONFIG_H_

#include "AbstractConfig.h"


class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv);
   
   private:

      // configurables

      std::string  storeTargetDirectory;

      // upgrade tool options
      std::string  targetIdMapFile;
      std::string  metaIdMapFile;
      std::string  storageIdMapFile; // optional, not required as format update

      // internals

      virtual void loadDefaults(bool addDashes);
      virtual bool applyConfigMap(bool enableException, bool addDashes);



   public:
      // getters & setters

      std::string getStoreTargetDirectory() const
      {
         return storeTargetDirectory;
      }

      std::string getTargetIdMapFile() const
      {
         return targetIdMapFile;
      }

      std::string getMetaIdMapFile() const
      {
         return metaIdMapFile;
      }

      std::string getStorageIdMapFile() const
      {
         return storageIdMapFile;
      }

      std::string getLogFile() const
      {
         return this->logStdFile;
      }

};

#endif /*CONFIG_H_*/
