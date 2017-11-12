#ifndef ABSTRACTAPP_H_
#define ABSTRACTAPP_H_

#include <common/app/log/Logger.h>
#include <common/app/config/ICommonConfig.h>
#include <common/net/message/AbstractNetMessageFactory.h>
#include <common/threading/PThread.h>
#include <common/toolkit/NetFilter.h>
#include <common/Common.h>


class AbstractApp : public PThread
{
   public:
      virtual ~AbstractApp() {}
      
      virtual void stopComponents() = 0;
      virtual void handleComponentException(std::exception& e) = 0;

      int createAndLockPIDFile(std::string pidFile) throw(InvalidConfigException);
      void updateLockedPIDFile(int pidFileFD) throw(InvalidConfigException);
      void unlockPIDFile(int pidFileFD);

      void waitForComponentTermination(PThread* component);
      
      virtual Logger* getLogger() = 0;
      virtual ICommonConfig* getCommonConfig() = 0;
      virtual NetFilter* getNetFilter() = 0;
      virtual AbstractNetMessageFactory* getNetMessageFactory() = 0;
      
   protected:
      AbstractApp() : PThread("Main", this) {}
};

#endif /*ABSTRACTAPP_H_*/
