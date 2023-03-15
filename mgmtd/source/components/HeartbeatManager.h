#ifndef HEARTBEATMANAGER_H_
#define HEARTBEATMANAGER_H_

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/net/message/NetMessage.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/NodeStoreClients.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/threading/PThread.h>
#include <common/threading/Condition.h>
#include <common/Common.h>
#include <components/componenthelpers/HbMgrNotification.h>
#include <components/DatagramListener.h>
#include <nodes/NodeStoreClientsEx.h>
#include <nodes/NodeStoreServersEx.h>


typedef std::list<HbMgrNotification*> HbMgrNotificationList;
typedef HbMgrNotificationList::iterator HbMgrNotificationListIter;


class HeartbeatManager : public PThread
{
   public:
      HeartbeatManager(DatagramListener* dgramLis);
      virtual ~HeartbeatManager();

      bool initRootNode(NumNodeID rootIDHint, bool rootIsBuddyMirrored);
      void notifyAsyncAddedNode(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType);
      void notifyAsyncRemovedNode(NumNodeID nodeNumID, NodeType nodeType);
      void notifyAsyncRefreshNode(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType);
      void notifyAsyncAddedTarget(uint16_t targetID, NumNodeID nodeID, StoragePoolId storagePoolId);
      void notifyAsyncAddedMirrorBuddyGroup(NodeType nodeType, uint16_t buddyGroupID,
         uint16_t primaryTargetID, uint16_t secondaryTargetID);
      void notifyAsyncRefreshCapacityPools();
      void notifyAsyncRefreshTargetStates();
      void notifyAsyncPublishCapacities();
      void notifyAsyncRefreshStoragePools();
      bool notifyNodes();
      void saveNodeStores();

   protected:
      LogContext log;
      Config* cfg;
      AbstractDatagramListener* dgramLis;


   private:
      NodeStoreClientsEx* clients;
      NodeStoreServersEx* metaNodes;
      NodeStoreServersEx* storageNodes;

      HbMgrNotificationList notificationList; // queue: new elems to back side
      Mutex notificationListMutex;


      void run();

      void requestLoop();

      void mgmtInit();

      void performClientsCheck();
      bool processNotificationList();

};

#endif /*HEARTBEATMANAGER_H_*/
