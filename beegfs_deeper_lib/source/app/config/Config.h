#ifndef CONFIG_H_
#define CONFIG_H_


#include <common/cache/app/config/CacheConfig.h>



class Config : public CacheConfig
{
   public:
      Config(int argc, char** argv);

   private:

      // configurable
      uint64_t sysCacheID;                     // the ID of the used DEEP-ER BeeGFS cache

      // internals
      virtual void loadDefaults(bool addDashes);
      virtual void applyConfigMap(bool enableException, bool addDashes);
      std::string createDefaultCfgFilename() const;
      void initImplicitVals();

   public:
      // getters & setters
      uint64_t getSysCacheID() const
      {
         return sysCacheID;
      }
};

#endif /*CONFIG_H_*/
