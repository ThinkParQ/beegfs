#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SocketTk.h>
#include <common/toolkit/TimeAbs.h>
#include <program/Program.h>
#include "HeartbeatManager.h"

#include <mutex>

HeartbeatManager::HeartbeatManager(DatagramListener* dgramLis)
 : PThread("HBeatMgr"),
   log("HBeatMgr"),
   cfg(Program::getApp()->getConfig() ),
   dgramLis(dgramLis),
   clients(Program::getApp()->getClientNodes() ),
   metaNodes(Program::getApp()->getMetaNodes() ),
   storageNodes(Program::getApp()->getStorageNodes() )
{
}

HeartbeatManager::~HeartbeatManager()
{
   while(!notificationList.empty() )
   {
      delete(notificationList.front() );
      notificationList.pop_front();
   }
}


void HeartbeatManager::run()
{
   try
   {
      registerSignalHandler();

      mgmtInit();

      requestLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }

   saveNodeStores();
}

void HeartbeatManager::requestLoop()
{
   const int sleepTimeMS = 5000;
   bool skipSleepThisTime = false;

   const unsigned storeSaveIntervalMS = 900000;
   const unsigned clientsIntervalMS = 60*1000;

   Time lastStoreSaveT;
   Time lastClientsT;


   while(!waitForSelfTerminateOrder(skipSleepThisTime ? 0 : sleepTimeMS) )
   {
      skipSleepThisTime = false;

      if(processNotificationList() )
         skipSleepThisTime = true;

      // save stores
      if(lastStoreSaveT.elapsedMS() > storeSaveIntervalMS)
      {
         saveNodeStores();

         lastStoreSaveT.setToNow();
      }

      // check clients
      if(lastClientsT.elapsedMS() > clientsIntervalMS)
      {
         performClientsCheck();

         lastClientsT.setToNow();
      }

   } // end of while loop
}

bool HeartbeatManager::notifyNodes()
{
   log.log(Log_NOTICE, "Notifying stored nodes...");

   const int numRetries = 1;
   const int timeoutMS = 200;

   App* app = Program::getApp();
   Node& localNode = app->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   NumNodeID rootNodeID = app->getMetaRoot().getID();
   NicAddressList nicList(localNode.getNicList() );

   HeartbeatMsg hbMsg(localNode.getID(), localNodeNumID, NODETYPE_Mgmt, &nicList);
   hbMsg.setRootNumID(rootNodeID);
   hbMsg.setPorts(cfg->getConnMgmtdPortUDP(), cfg->getConnMgmtdPortTCP() );

   dgramLis->sendToNodesUDP(storageNodes, &hbMsg, numRetries, timeoutMS);
   dgramLis->sendToNodesUDP(metaNodes, &hbMsg, numRetries, timeoutMS);
   dgramLis->sendToNodesUDP(clients, &hbMsg, numRetries, timeoutMS);

   // assume success?
   return true;
}

void HeartbeatManager::mgmtInit()
{
   notifyNodes();
   log.log(Log_NOTICE, "Init complete.");
}


/**
 * @param rootIDHint empty string to auto-define root or a nodeID that is assumed to be the root
 * @return true if a new root node has been defined
 */
bool HeartbeatManager::initRootNode(NumNodeID rootIDHint, bool rootIsBuddyMirrored)
{
   // be careful: this method is also called from other threads
   // note: after this method, the root node might still be undefined (this is normal)

   bool setRootRes = false;

   if (rootIDHint || metaNodes->getSize())
   { // check whether root has already been set

      if (!rootIDHint)
         rootIDHint = metaNodes->getLowestNodeID();

      // set root to lowest ID (if no other root was set yet)
      setRootRes = Program::getApp()->getMetaRoot().setIfDefault(rootIDHint, rootIsBuddyMirrored);

      if(setRootRes)
      { // new root set
         log.log(Log_CRITICAL, "New root directory metadata node: " +
            Program::getApp()->getMetaNodes()->getNodeIDWithTypeStr(rootIDHint) );

         notifyAsyncAddedNode("", rootIDHint, NODETYPE_Meta); /* (real string ID will
            be retrieved by notifier before sending the heartbeat) */
      }
   }

   return setRootRes;
}


/**
 * Check reachability of clients and remove unreachable clients based on
 * cfg->getTuneClientAutoRemoveMins
 */
void HeartbeatManager::performClientsCheck()
{
   unsigned autoRemoveMins = cfg->getTuneClientAutoRemoveMins(); // 0 means disabled

   if(!autoRemoveMins)
      return;

   for (const auto& node : clients->referenceAllNodes())
   {
      const unsigned hbElapsedMins = node->getLastHeartbeatT().elapsedMS() / (1000*60);

      if (hbElapsedMins < autoRemoveMins)
         continue;

      clients->deleteNode(node->getNumID());

      LOG(GENERAL, WARNING, "Node is not responding and will be removed.",
            ("node", node->getNodeIDWithTypeStr()),
            ("remaining nodes", clients->getSize()));

      // add removed node to notification list
      notifyAsyncRemovedNode(node->getNumID(), NODETYPE_Client);
   }
}

void HeartbeatManager::notifyAsyncAddedNode(std::string nodeID, NumNodeID nodeNumID,
   NodeType nodeType)
{
   HbMgrNotification* notification = new HbMgrNotificationNodeAdded(nodeID, nodeNumID, nodeType);

   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}

void HeartbeatManager::notifyAsyncRemovedNode(NumNodeID nodeNumID,
   NodeType nodeType)
{
   HbMgrNotification* notification = new HbMgrNotificationNodeRemoved(nodeNumID, nodeType);

   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}

void HeartbeatManager::notifyAsyncRefreshNode(std::string nodeID, NumNodeID nodeNumID,
   NodeType nodeType)
{
   HbMgrNotification* notification = new HbMgrNotificationRefreshNode(nodeID, nodeNumID, nodeType);

   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}

void HeartbeatManager::notifyAsyncAddedTarget(uint16_t targetID, NumNodeID nodeID,
   StoragePoolId storagePoolId)
{
   HbMgrNotification* notification =
         new HbMgrNotificationTargetAdded(targetID, nodeID, storagePoolId);

   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}

void HeartbeatManager::notifyAsyncAddedMirrorBuddyGroup(NodeType nodeType, uint16_t buddyGroupID,
   uint16_t primaryTargetID, uint16_t secondaryTargetID)
{
   HbMgrNotification* notification = new HbMgrNotificationMirrorBuddyGroupAdded(nodeType,
      buddyGroupID, primaryTargetID, secondaryTargetID);

   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}


void HeartbeatManager::notifyAsyncRefreshCapacityPools()
{
   HbMgrNotification* notification = new HbMgrNotificationRefreshCapacityPools();

   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}

void HeartbeatManager::notifyAsyncRefreshTargetStates()
{
   HbMgrNotification* notification = new HbMgrNotificationRefreshTargetStates();

   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}

/**
 * Notify all the storage and metadata nodes to publish their free capacity info.
 */
void HeartbeatManager::notifyAsyncPublishCapacities()
{
   log.log(Log_DEBUG, "Asking nodes to publish capacity info...");

   HbMgrNotification* notification = new HbMgrNotificationPublishCapacities();
   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}

/**
 * Notify all the and metadata nodes that storage pools need refreshment
 */
void HeartbeatManager::notifyAsyncRefreshStoragePools()
{
   LOG(STORAGEPOOLS, DEBUG, "Asking nodes to publish storage target pools info...");

   HbMgrNotification* notification = new HbMgrNotificationRefreshStoragePools();
   const std::lock_guard<Mutex> lock (notificationListMutex);
   notificationList.push_back(notification);
}

/**
 * Note: Processes only a single (!) entry each time it is called (to avoid long locking).
 *
 * @return true if more events are waiting in the queue
 */
bool HeartbeatManager::processNotificationList()
{
   std::unique_lock<Mutex> listLock(notificationListMutex); // L O C K

   if(!notificationList.empty() )
   {
      HbMgrNotification* currentNotification = *notificationList.begin();

      notificationList.pop_front();

      /* note: we unlock to avoid blocking other (worker) threads that add new notifications on our
         communication. it's safe because the front of the list won't change. */

      listLock.unlock(); // U N L O C K

      currentNotification->processNotification();

      listLock.lock(); // R E L O C K

      delete(currentNotification);
   }

   return !notificationList.empty();
}

void HeartbeatManager::saveNodeStores()
{
   metaNodes->saveToFile(&Program::getApp()->getMetaRoot());
   storageNodes->saveToFile(nullptr);
   clients->saveToFile();
}
