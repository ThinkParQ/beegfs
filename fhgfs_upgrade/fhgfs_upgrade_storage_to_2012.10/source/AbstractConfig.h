#ifndef ABSTRACTCONFIG_H_
#define ABSTRACTCONFIG_H_


#include "Common.h"
#include "MapTk.h"
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

      bool isValid;

      // configurables

      AbstractConfig(int argc, char** argv);

      void initConfig(int argc, char** argv, bool enableException, bool addDashes=false);
      StringMapIter eraseFromConfigMap(StringMapIter iter);

      virtual void loadDefaults(bool addDashes=false);
      virtual bool applyConfigMap(bool enableException, bool addDashes=false);
      virtual void initImplicitVals();

      void initInterfacesList(std::string connInterfacesFile, std::string& inoutConnInterfacesList);
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

      bool getIsValidConfig()
      {
         return this->isValid;
      }

};

#endif /*ABSTRACTCONFIG_H_*/
