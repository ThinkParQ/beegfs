#ifndef ABSTRACTCONFIG_H_
#define ABSTRACTCONFIG_H_


#include <common/Common.h>
#include <common/toolkit/MapTk.h>
#include "InvalidConfigException.h"
#include "ConnAuthFileException.h"
#include "ICommonConfig.h"



class AbstractConfig : public ICommonConfig
{
   public:
      virtual ~AbstractConfig() {}

   protected:
      // internals

      StringMap configMap;
      int argc;
      char** argv;

      void addLineToConfigMap(std::string line);
      void loadFromArgs(int argc, char** argv);

      // configurables

      std::string cfgFile;

      AbstractConfig(int argc, char** argv);

      void initConfig(int argc, char** argv, bool enableException, bool addDashes=false);
      StringMapIter eraseFromConfigMap(StringMapIter iter);

      virtual void loadDefaults(bool addDashes=false);
      void loadFromFile(const char* filename, bool addDashes=false);
      virtual void applyConfigMap(bool enableException, bool addDashes=false);
      virtual void initImplicitVals();

      void initInterfacesList(const std::string& connInterfacesFile,
                              std::string& inoutConnInterfacesList);
      void initConnAuthHash(const std::string& connAuthFile, uint64_t* outConnAuthHash);
      void initSocketBufferSizes();

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
      bool testConfigMapKeyMatch(const StringMapCIter& iter, const std::string& testKey,
         bool addDashesToTestKey) const
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
      const StringMap* getConfigMap() const
      {
         return &configMap;
      }

      int getArgc() const
      {
         return argc;
      }

      char** getArgv() const
      {
         return argv;
      }

      static void assignKeyIfNotZero(const StringMapIter&, int& intVal, bool enableException = true);

   public:
      std::string getCfgFile() const
      {
         return cfgFile;
      }

};

#endif /*ABSTRACTCONFIG_H_*/
