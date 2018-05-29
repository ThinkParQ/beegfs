#ifndef NODELISTREQUESTOR_H_
#define NODELISTREQUESTOR_H_

/*
 * NodeListRequestor generates work packets (in fixed intervals), which query the management node
 * for a list of nodes in the system
 */

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/NodeStore.h>
#include <common/threading/PThread.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>
#include <common/Common.h>
#include <components/DatagramListener.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/RemoveNodeMsg.h>

#include <common/net/message/nodes/GetNodesMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SocketTk.h>
#include <common/toolkit/NodesTk.h>
#include <nodes/NodeStoreMetaEx.h>
#include <nodes/NodeStoreStorageEx.h>
#include <nodes/NodeStoreMgmtEx.h>
#include <components/worker/GetMetaNodesWork.h>
#include <components/worker/GetClientNodesWork.h>
#include <components/worker/GetStorageNodesWork.h>
#include <common/threading/Condition.h>


#define MGMT_HEARTBEAT_NUM_RETRIES 2
#define MGMT_HEARTBEAT_TIMEOUT_MS 1000
#define REQUEST_LOOP_INTERVAL_MS 30000


class NodeListRequestor: public PThread
{
   public:
      NodeListRequestor();

      virtual ~NodeListRequestor()
      {
      }

   protected:
      Mutex waitForTimeoutMutex;
      Condition waitForTimeout;
      bool workersFinished;
      LogContext log;
      Config* cfg;
      AbstractDatagramListener* dgramLis;
      NodeStoreMetaEx* metaNodes;
      NodeStoreStorageEx* storageNodes;
      NodeStoreMgmtEx* mgmtNodes;
      NodeStoreClients* clientNodes;
      MirrorBuddyGroupMapper* metaBuddyGroups;
      MirrorBuddyGroupMapper* storageBuddyGroups;
      MultiWorkQueue* workQueue;

   private:
      void run();
      void requestLoop();
      bool getMgmtNodeInfo();
};

#endif /*NODELISTREQUESTOR_H_*/
