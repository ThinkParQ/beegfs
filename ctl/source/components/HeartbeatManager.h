#ifndef HEARTBEATMANAGER_H_
#define HEARTBEATMANAGER_H_

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/threading/PThread.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>
#include <common/Common.h>
#include <components/DatagramListener.h>

class HeartbeatManager : public PThread
{
   public:
      HeartbeatManager(DatagramListener* dgramLis);
      virtual ~HeartbeatManager();


   protected:
      LogContext log;
      Config* cfg;
      AbstractDatagramListener* dgramLis;


   private:
      NodeStoreServers* metaNodes;
      NodeStoreServers* storageNodes;

      void run();

      virtual void requestLoop();

};

#endif /*HEARTBEATMANAGER_H_*/
