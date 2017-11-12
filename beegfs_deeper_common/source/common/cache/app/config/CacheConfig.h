#ifndef CACHECONFIG_H_
#define CACHECONFIG_H_

#include <common/app/config/AbstractConfig.h>


#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-deeper-lib.conf"
#define CONFIG_DEFAULT_NAMEDSOCKETPATH "/var/run/beegfs-deeper-cached.sock"

#define IGNORE_CONFIG_VALUE(keyStr) /* to be used in applyConfigMap() */ \
   if(testConfigMapKeyMatch(iter, keyStr, addDashes) ) \
      ; \
   else

class CacheConfig : public AbstractConfig
{
   public:
      CacheConfig(int argc, char** argv);

   private:

      // configurable

      std::string sysMountPointCache;          // mount point of the DEEP-ER BeeGFS cache
      std::string sysMountPointGlobal;         // mount point of the global BeeGFS storage

      std::string connNamedSocket; // the path to the named socket of the DEEP-ER cached


   protected:
      virtual void loadDefaults(bool addDashes=false);
      virtual void applyConfigMap(bool enableException, bool addDashes=false);
      std::string createDefaultCfgFilename() const;
      virtual void initImplicitVals();



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

      const std::string& getConnNamedSocket() const
      {
         return connNamedSocket;
      }
};

#endif /*CACHECONFIG_H_*/
