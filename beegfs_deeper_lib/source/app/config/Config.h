#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>



class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv) throw(InvalidConfigException);
   
   private:

      // configurable

      std::string sysMountPointCache;          // mount point of the DEEP-ER BeeGFS cache
      std::string sysMountPointGlobal;         // mount point of the global BeeGFS storage

      uint64_t sysCacheID;                     // the ID of the used DEEP-ER BeeGFS cache

      std::string connDeeperCachedNamedSocket; // the path to the named socket of the DEEP-ER cached


      // internals

      virtual void loadDefaults(bool addDashes);
      virtual void applyConfigMap(bool enableException, bool addDashes)
         throw(InvalidConfigException);
      std::string createDefaultCfgFilename();
      void initImplicitVals() throw(InvalidConfigException);


      
   public:
      // getters & setters

      const std::string& getSysMountPointCache() const
      {
         return sysMountPointCache;
      }
      
      const std::string& getSysMountPointGlobal() const
      {
         return sysMountPointGlobal;
      }

      const uint64_t& getSysCacheID() const
      {
         return sysCacheID;
      }

      const std::string& getConnDeeperCachedNamedSocket() const
      {
         return connDeeperCachedNamedSocket;
      }
};

#endif /*CONFIG_H_*/
