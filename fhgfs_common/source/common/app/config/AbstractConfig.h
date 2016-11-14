#ifndef ABSTRACTCONFIG_H_
#define ABSTRACTCONFIG_H_


#include <common/Common.h>
#include <common/toolkit/MapTk.h>
#include "InvalidConfigException.h"
#include "ICommonConfig.h"



class AbstractConfig : public ICommonConfig
{
   public:
      virtual ~AbstractConfig() {}
      
   private:
      // internals

      StringMap configMap;
      int argc;
      char** argv;
      
      void addLineToConfigMap(std::string line);
      void loadFromArgs(int argc, char** argv);
      
   protected:
      // configurables

      std::string cfgFile;

      AbstractConfig(int argc, char** argv) throw(InvalidConfigException);

      void initConfig(int argc, char** argv, bool enableException, bool addDashes=false)
         throw(InvalidConfigException);
      StringMapIter eraseFromConfigMap(StringMapIter iter);

      virtual void loadDefaults(bool addDashes=false);
      void loadFromFile(const char* filename, bool addDashes=false) throw(InvalidConfigException);
      virtual void applyConfigMap(bool enableException, bool addDashes=false)
         throw(InvalidConfigException);
      virtual void initImplicitVals() throw(InvalidConfigException);

      void initInterfacesList(std::string connInterfacesFile, std::string& inoutConnInterfacesList)
         throw(InvalidConfigException);
      void initConnAuthHash(std::string connAuthFile, uint64_t* outConnAuthHash)
         throw(InvalidConfigException);

      // inliners

      /**
       * @param addDashes true to prepend "--" to the keyStr.
       */
      void configMapRedefine(std::string keyStr, std::string valueStr, bool addDashes=false)
      {
         std::string keyStrInternal;

         if(addDashes)
            keyStrInternal = "--" + keyStr;
         else
            keyStrInternal = keyStr;

         MapTk::stringMapRedefine(keyStrInternal, valueStr, &configMap);
      }
      
      /**
       * Note: Read the addDashesToTestKey comment on case-sensitivity.
       *
       * @param addDashesToTestKey true to prepend "--" to testKey before testing for match; if this
       * is specified, the check will also be case-insensitive (otherwise it is case-sensitive).
       * @return true if iter->first equals testKey.
       */
      bool testConfigMapKeyMatch(StringMapIter& iter, std::string testKey, bool addDashesToTestKey)
      {
         if(addDashesToTestKey)
         {
            std::string testKeyDashed = "--" + testKey;
            return (!strcasecmp(iter->first.c_str(), testKeyDashed.c_str() ) );
         }
         else
            return (iter->first == testKey);
      }

      // getters & setters
      StringMap* getConfigMap()
      {
         return &configMap;
      }
      
      int getArgc()
      {
         return argc;
      }
      
      char** getArgv()
      {
         return argv;
      }
      
   public:
      std::string getCfgFile()
      {
         return cfgFile;
      }

};

#endif /*ABSTRACTCONFIG_H_*/
