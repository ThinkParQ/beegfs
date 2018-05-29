#ifndef ABSTRACTAPP_H_
#define ABSTRACTAPP_H_

#include <common/app/log/Logger.h>
#include <common/app/config/ICommonConfig.h>
#include <common/net/message/AbstractNetMessageFactory.h>
#include <common/threading/PThread.h>
#include <common/toolkit/BitStore.h>
#include <common/toolkit/NetFilter.h>
#include <common/Common.h>


#define APP_FEATURES_ARRAY_LEN      (sizeof(APP_FEATURES)/sizeof(unsigned) ) /* APP_FEATURES is
                                                                        defined in App.cpp files */


class StreamListenerV2; // forward declaration


class AbstractApp : public PThread
{
   public:
      virtual void stopComponents() = 0;
      virtual void handleComponentException(std::exception& e) = 0;

      int createAndLockPIDFile(std::string pidFile);
      void updateLockedPIDFile(int pidFileFD);
      void unlockAndDeletePIDFile(int pidFileFD, const std::string& pidFile);

      void waitForComponentTermination(PThread* component);

      static void handleOutOfMemFromNew();

      virtual ICommonConfig* getCommonConfig() = 0;
      virtual NetFilter* getNetFilter() = 0;
      virtual NetFilter* getTcpOnlyFilter() = 0;
      virtual AbstractNetMessageFactory* getNetMessageFactory() = 0;

      static bool didRunTimeInit;


      /**
       * To be overridden by Apps that support multiple stream listeners.
       */
      virtual StreamListenerV2* getStreamListenerByFD(int fd)
      {
         return NULL;
      }

   protected:
      AbstractApp() : PThread("Main", this)
      {
         if (!didRunTimeInit)
         {
            std::cerr << "Bug: Runtime variables have not been initialized" << std::endl;
            throw InvalidConfigException("Bug: Runtime variables have not been initialized");
         }

         std::set_new_handler(handleOutOfMemFromNew);
      }

      virtual ~AbstractApp()
      {
         basicDestructions();
      }

      static unsigned featuresGetHighestNum(const unsigned* featuresArray, unsigned numArrayElems);
      static void featuresToBitStore(const unsigned* featuresArray, unsigned numArrayElems,
         BitStore* outBitStore);


   private:
      static bool basicInitializations();
      bool basicDestructions();
      static bool performBasicInitialRunTimeChecks();


      // inliners

   public:

      /**
       * Note: This MUST be called before creating a new App
       */
      static bool runTimeInitsAndChecks()
      {
         bool runTimeChecks = performBasicInitialRunTimeChecks();
         if (!runTimeChecks)
            return false;

         bool initRes = basicInitializations();
         if (!initRes)
            return false;

         didRunTimeInit = true;

         return true;
      }
};

#endif /*ABSTRACTAPP_H_*/
