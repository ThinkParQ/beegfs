#include <app/App.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <common/net/message/control/AckMsgEx.h>
#include <common/net/message/nodes/HeartbeatMsgEx.h>
#include <common/net/message/nodes/HeartbeatRequestMsgEx.h>
#include <common/net/message/nodes/RegisterNodeMsg.h>
#include <common/net/message/nodes/RegisterNodeRespMsg.h>
#include <common/net/message/nodes/RemoveNodeMsgEx.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/Time.h>
#include "InternodeSyncer.h"


void InternodeSyncer_init(InternodeSyncer* this, App* app)
{
   // call super constructor
   Thread_init( (Thread*)this, BEEGFS_THREAD_NAME_PREFIX_STR "XNodeSync", __InternodeSyncer_run);

   this->forceTargetStatesUpdate = false;

   this->app = app;
   this->cfg = App_getConfig(app);

   this->dgramLis = App_getDatagramListener(app);
   this->helperd = App_getHelperd(app);

   this->mgmtNodes = App_getMgmtNodes(app);
   this->metaNodes = App_getMetaNodes(app);
   this->storageNodes = App_getStorageNodes(app);

   this->nodeRegistered = false;
   this->mgmtInitDone = false;

   Mutex_init(&this->mgmtInitDoneMutex);
   Condition_init(&this->mgmtInitDoneCond);

   Mutex_init(&this->delayedCloseMutex);
   PointerList_init(&this->delayedCloseQueue);

   Mutex_init(&this->delayedEntryUnlockMutex);
   PointerList_init(&this->delayedEntryUnlockQueue);

   Mutex_init(&this->delayedRangeUnlockMutex);
   PointerList_init(&this->delayedRangeUnlockQueue);

   Mutex_init(&this->forceTargetStatesUpdateMutex);
   Time_init(&this->lastSuccessfulTargetStatesUpdateT);
   this->targetOfflineTimeoutMS = Config_getSysTargetOfflineTimeoutSecs(this->cfg) * 1000;
}

struct InternodeSyncer* InternodeSyncer_construct(App* app)
{
   struct InternodeSyncer* this = (InternodeSyncer*)os_kmalloc(sizeof(*this) );

   if(likely(this) )
      InternodeSyncer_init(this, app);

   return this;
}

void InternodeSyncer_uninit(InternodeSyncer* this)
{
   PointerListIter iter;

   // free delayed close Q elements

   PointerListIter_init(&iter, &this->delayedCloseQueue);

   while(!PointerListIter_end(&iter) )
   {
      __InternodeSyncer_delayedCloseFreeEntry(this,
         (DelayedCloseEntry*)PointerListIter_value(&iter) );
      PointerListIter_next(&iter);
   }

   PointerList_uninit(&this->delayedCloseQueue);
   Mutex_uninit(&this->delayedCloseMutex);

   // free delayed entry unlock Q elements

   PointerListIter_init(&iter, &this->delayedEntryUnlockQueue);

   while(!PointerListIter_end(&iter) )
   {
      __InternodeSyncer_delayedEntryUnlockFreeEntry(this,
         (DelayedEntryUnlockEntry*)PointerListIter_value(&iter) );
      PointerListIter_next(&iter);
   }

   PointerList_uninit(&this->delayedEntryUnlockQueue);
   Mutex_uninit(&this->delayedEntryUnlockMutex);

   // free delayed range unlock Q elements

   PointerListIter_init(&iter, &this->delayedRangeUnlockQueue);

   while(!PointerListIter_end(&iter) )
   {
      __InternodeSyncer_delayedRangeUnlockFreeEntry(this,
         (DelayedRangeUnlockEntry*)PointerListIter_value(&iter) );
      PointerListIter_next(&iter);
   }

   PointerList_uninit(&this->delayedRangeUnlockQueue);
   Mutex_uninit(&this->delayedRangeUnlockMutex);

   Mutex_uninit(&this->forceTargetStatesUpdateMutex);

   Mutex_uninit(&this->mgmtInitDoneMutex);

   Thread_uninit( (Thread*)this);
}

void InternodeSyncer_destruct(InternodeSyncer* this)
{
   InternodeSyncer_uninit(this);

   kfree(this);
}

void __InternodeSyncer_run(Thread* this)
{
   InternodeSyncer* thisCast = (InternodeSyncer*)this;

   const char* logContext = "InternodeSyncer (run)";
   Logger* log = App_getLogger(thisCast->app);

   Logger_logFormatted(log, Log_DEBUG, logContext, "Searching for nodes...");

   __InternodeSyncer_mgmtInit(thisCast);

   _InternodeSyncer_requestLoop(thisCast);

   __InternodeSyncer_signalMgmtInitDone(thisCast);

   if(thisCast->nodeRegistered)
      __InternodeSyncer_unregisterNode(thisCast);

   Logger_log(log, Log_DEBUG, logContext, "Component stopped.");
}

void _InternodeSyncer_requestLoop(InternodeSyncer* this)
{
   const unsigned sleepTimeMS = 5*1000;

   const unsigned mgmtInitIntervalMS = 5000;
   const unsigned reregisterIntervalMS = 60*1000; /* send heartbeats to mgmt in this interval. must
      be a lot lower than tuneClientAutoRemoveMins of mgmt. the value is capped to at least 5
      (or disabled) on mgmt. */
   const unsigned downloadNodesIntervalMS = 180000;
   const unsigned updateTargetStatesMS = Config_getSysUpdateTargetStatesSecs(this->cfg) * 1000;
   const unsigned delayedOpsIntervalMS = 60*1000;
   const unsigned idleDisconnectIntervalMS = 70*60*1000; /* 70 minutes (must be
      less than half the server-side streamlis idle disconnect interval to avoid
      server disconnecting first) */
   const unsigned checkNetworkIntervalMS = 60*1000; // 1 minute

   Time lastMgmtInitT;
   Time lastReregisterT;
   Time lastDownloadNodesT;

   Time lastDelayedOpsT;
   Time lastIdleDisconnectT;
   Time lastTargetStatesUpdateT;
   Time lastCheckNetworkT;

   Thread* thisThread = (Thread*)this;

   Time_init(&lastMgmtInitT);
   Time_init(&lastReregisterT);
   Time_init(&lastDownloadNodesT);

   Time_init(&lastDelayedOpsT);
   Time_init(&lastIdleDisconnectT);
   Time_init(&lastTargetStatesUpdateT);
   Time_init(&lastCheckNetworkT);

   while(!_Thread_waitForSelfTerminateOrder(thisThread, sleepTimeMS) )
   {
      bool targetStatesUpdateForced =
         InternodeSyncer_getAndResetForceTargetStatesUpdate(this);

      // mgmt init
      if(!this->mgmtInitDone)
      {
         if(Time_elapsedMS(&lastMgmtInitT) > mgmtInitIntervalMS)
         {
            __InternodeSyncer_mgmtInit(this);

            Time_setToNow(&lastMgmtInitT);
         }

         continue;
      }

      // everything below only happens after successful management init...

      // check for NIC changes
      if(Time_elapsedMS(&lastCheckNetworkT) > checkNetworkIntervalMS)
      {
         if (__InternodeSyncer_checkNetwork(this))
            Time_setZero(&lastReregisterT);
         Time_setToNow(&lastCheckNetworkT);
      }

      // re-register
      if(Time_elapsedMS(&lastReregisterT) > reregisterIntervalMS)
      {
         __InternodeSyncer_reregisterNode(this);

         Time_setToNow(&lastReregisterT);
      }

      // download & sync nodes
      if(Time_elapsedMS(&lastDownloadNodesT) > downloadNodesIntervalMS)
      {
         __InternodeSyncer_downloadAndSyncNodes(this);
         __InternodeSyncer_downloadAndSyncTargetMappings(this);

         Time_setToNow(&lastDownloadNodesT);
      }

      // download target states
      if( targetStatesUpdateForced ||
          (Time_elapsedMS(&lastTargetStatesUpdateT) > updateTargetStatesMS) )
      {
         __InternodeSyncer_updateTargetStatesAndBuddyGroups(this, NODETYPE_Meta);
         __InternodeSyncer_updateTargetStatesAndBuddyGroups(this, NODETYPE_Storage);

         Time_setToNow(&lastTargetStatesUpdateT);
      }

      // delayed operations (that were left due to interruption by signal or network errors)
      if(Time_elapsedMS(&lastDelayedOpsT) > delayedOpsIntervalMS)
      {
         __InternodeSyncer_delayedEntryUnlockComm(this);
         __InternodeSyncer_delayedRangeUnlockComm(this);
         __InternodeSyncer_delayedCloseComm(this);

         Time_setToNow(&lastDelayedOpsT);
      }

      // drop idle connections
      if(Time_elapsedMS(&lastIdleDisconnectT) > idleDisconnectIntervalMS)
      {
         __InternodeSyncer_dropIdleConns(this);

         Time_setToNow(&lastIdleDisconnectT);
      }
   }
}

void __InternodeSyncer_signalMgmtInitDone(InternodeSyncer* this)
{
   Mutex_lock(&this->mgmtInitDoneMutex);

   this->mgmtInitDone = true;

   Condition_broadcast(&this->mgmtInitDoneCond);

   Mutex_unlock(&this->mgmtInitDoneMutex);
}

void __InternodeSyncer_mgmtInit(InternodeSyncer* this)
{
   static bool waitForMgmtLogged = false; // to avoid log spamming

   const char* logContext = "Init";
   Logger* log = App_getLogger(this->app);

   if(!waitForMgmtLogged)
   {
      const char* hostname = Config_getSysMgmtdHost(this->cfg);
      unsigned short port = Config_getConnMgmtdPortUDP(this->cfg);

      Logger_logFormatted(log, Log_WARNING, logContext,
         "Waiting for beegfs-mgmtd@%s:%hu...", hostname, port);
      waitForMgmtLogged = true;
   }

   if(!__InternodeSyncer_waitForMgmtHeartbeat(this) )
      return;

   Logger_log(log, Log_NOTICE, logContext, "Management node found. Downloading node groups...");

   __InternodeSyncer_downloadAndSyncNodes(this);
   __InternodeSyncer_downloadAndSyncTargetMappings(this);
   __InternodeSyncer_updateTargetStatesAndBuddyGroups(this, NODETYPE_Storage);
   __InternodeSyncer_updateTargetStatesAndBuddyGroups(this, NODETYPE_Meta);

   Logger_log(log, Log_NOTICE, logContext, "Node registration...");

   if(__InternodeSyncer_registerNode(this) )
   { // download nodes again now that we will receive notifications about add/remove (avoids race)
      __InternodeSyncer_downloadAndSyncNodes(this);
      __InternodeSyncer_downloadAndSyncTargetMappings(this);
      __InternodeSyncer_updateTargetStatesAndBuddyGroups(this, NODETYPE_Storage);
      __InternodeSyncer_updateTargetStatesAndBuddyGroups(this, NODETYPE_Meta);
   }

   __InternodeSyncer_signalMgmtInitDone(this);

   Logger_log(log, Log_NOTICE, logContext, "Init complete.");
}


/**
 * @param timeoutMS 0 to wait infinitely (which is probably never what you want)
 */
bool InternodeSyncer_waitForMgmtInit(InternodeSyncer* this, int timeoutMS)
{
   bool retVal;

   Mutex_lock(&this->mgmtInitDoneMutex);

   if(!timeoutMS)
   {
      while(!this->mgmtInitDone)
            Condition_wait(&this->mgmtInitDoneCond, &this->mgmtInitDoneMutex);
   }
   else
   if(!this->mgmtInitDone)
      Condition_timedwait(&this->mgmtInitDoneCond, &this->mgmtInitDoneMutex, timeoutMS);

   retVal = this->mgmtInitDone;

   Mutex_unlock(&this->mgmtInitDoneMutex);

   return retVal;
}

bool __InternodeSyncer_waitForMgmtHeartbeat(InternodeSyncer* this)
{
   const int waitTimeoutMS = 400;
   const int numRetries = 1;

   char heartbeatReqBuf[NETMSG_MIN_LENGTH];
   Thread* thisThread = (Thread*)this;

   const char* hostname = Config_getSysMgmtdHost(this->cfg);
   unsigned short port = Config_getConnMgmtdPortUDP(this->cfg);
   struct in_addr ipAddr;
   int i;

   HeartbeatRequestMsgEx msg;

   if(NodeStoreEx_getSize(this->mgmtNodes) )
      return true;


   // prepare request message

   HeartbeatRequestMsgEx_init(&msg);
   NetMessage_serialize( (NetMessage*)&msg, heartbeatReqBuf, NETMSG_MIN_LENGTH);


   // resolve name, send message, wait for incoming heartbeat

   for(i=0; (i <= numRetries) && !Thread_getSelfTerminate(thisThread); i++)
   {
      bool resolveRes = SocketTk_getHostByName(this->helperd, hostname, &ipAddr);
      if(resolveRes)
      { // we got an ip => send request and wait
         int tryTimeoutWarpMS = Random_getNextInRange(-(waitTimeoutMS/4), waitTimeoutMS/4);

         DatagramListener_sendtoIP_kernel(this->dgramLis, heartbeatReqBuf, sizeof heartbeatReqBuf, 0,
            ipAddr, port);

         if(NodeStoreEx_waitForFirstNode(this->mgmtNodes, waitTimeoutMS + tryTimeoutWarpMS) )
         {
            return true; // heartbeat received => we're done
         }
      }
   }

   return false;
}

/**
 * @return true if an ack was received for the heartbeat, false otherwise
 */
bool __InternodeSyncer_registerNode(InternodeSyncer* this)
{
   static bool registrationFailureLogged = false; // to avoid log spamming

   const char* logContext = "Registration";
   Logger* log = App_getLogger(this->app);

   Node* mgmtNode;

   NoAllocBufferStore* bufStore = App_getMsgBufStore(this->app);
   Node* localNode = App_getLocalNode(this->app);
   const char* localNodeID = Node_getID(localNode);
   NumNodeID localNodeNumID = Node_getNumID(localNode);
   NumNodeID newLocalNodeNumID;
   NicAddressList nicList;

   RegisterNodeMsg msg;
   char* respBuf;
   NetMessage* respMsg;
   FhgfsOpsErr requestRes;
   RegisterNodeRespMsg* registerResp;
   bool result;

   Node_cloneNicList(localNode, &nicList);
   mgmtNode = NodeStoreEx_referenceFirstNode(this->mgmtNodes);
   if(!mgmtNode)
   {
      result = false;
      goto exit;
   }

   RegisterNodeMsg_initFromNodeData(&msg, localNodeID, localNodeNumID, NODETYPE_Client,
      &nicList, Config_getConnClientPortUDP(this->cfg));

   // connect & communicate
   requestRes = MessagingTk_requestResponse(this->app, mgmtNode,
      (NetMessage*)&msg, NETMSGTYPE_RegisterNodeResp, &respBuf, &respMsg);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // request/response failed
      goto cleanup_request;
   }

   // handle result
   registerResp = (RegisterNodeRespMsg*)respMsg;
   newLocalNodeNumID = RegisterNodeRespMsg_getNodeNumID(registerResp);

   if (newLocalNodeNumID.value == 0)
   {
      Logger_log(log, Log_CRITICAL, logContext,
         "Unable to register at management daemon. No valid numeric node ID retrieved.");
   }
   else
   {
      Node_setNumID(localNode, newLocalNodeNumID);

      this->nodeRegistered = true;
   }

   // clean-up
   NETMESSAGE_FREE(respMsg);
   NoAllocBufferStore_addBuf(bufStore, respBuf);

cleanup_request:
   Node_put(mgmtNode);

   // log registration result
   if(this->nodeRegistered)
      Logger_log(log, Log_WARNING, logContext, "Node registration successful.");
   else
   if(!registrationFailureLogged)
   {
      Logger_log(log, Log_CRITICAL, logContext,
         "Node registration failed. Management node offline? Will keep on trying...");
      registrationFailureLogged = true;
   }

   result = this->nodeRegistered;

exit:

   ListTk_kfreeNicAddressListElems(&nicList);
   NicAddressList_uninit(&nicList);

   return result;
}

bool __InternodeSyncer_checkNetwork(InternodeSyncer* this)
{
   Logger* log = App_getLogger(this->app);
   NicAddressList newNicList;
   NodeConnPool* connPool = Node_getConnPool(App_getLocalNode(this->app));
   bool result = false;

   if (App_findAllowedInterfaces(this->app, &newNicList))
   {
      NodeConnPool_lock(connPool);
      result = !NicAddressList_equals(&newNicList, NodeConnPool_getNicListLocked(connPool));
      NodeConnPool_unlock(connPool);
      if (result)
      {
         Logger_log(log, Log_NOTICE, "checkNetwork", "Local interfaces have changed.");
         App_updateLocalInterfaces(this->app, &newNicList);
      }
      ListTk_kfreeNicAddressListElems(&newNicList);
      NicAddressList_uninit(&newNicList);
   }
   return result;
}

/**
 * Note: This just sends a heartbeat to the mgmt node to re-new our existence information
 * (in case the mgmt node assumed our death for some reason like network errors etc.)
 */
void __InternodeSyncer_reregisterNode(InternodeSyncer* this)
{
   Node* mgmtNode;

   Node* localNode = App_getLocalNode(this->app);
   const char* localNodeID = Node_getID(localNode);
   NumNodeID localNodeNumID = Node_getNumID(localNode);
   NicAddressList nicList;

   HeartbeatMsgEx msg;

   Node_cloneNicList(localNode, &nicList);

   mgmtNode = NodeStoreEx_referenceFirstNode(this->mgmtNodes);
   if(!mgmtNode)
      goto exit;

   HeartbeatMsgEx_initFromNodeData(&msg, localNodeID, localNodeNumID, NODETYPE_Client, &nicList);
   HeartbeatMsgEx_setPorts(&msg, Config_getConnClientPortUDP(this->cfg), 0);

   DatagramListener_sendMsgToNode(this->dgramLis, mgmtNode, (NetMessage*)&msg);

   Node_put(mgmtNode);

exit:

   ListTk_kfreeNicAddressListElems(&nicList);
   NicAddressList_uninit(&nicList);

}

/**
 * @return true if an ack was received for the heartbeat, false otherwise
 */
bool __InternodeSyncer_unregisterNode(InternodeSyncer* this)
{
   const char* logContext = "Deregistration";
   Logger* log = App_getLogger(this->app);

   /* note: be careful not to use datagrams here, because this is called during App stop and hence
         the DGramLis is probably not even listening for responses anymore */

   Node* mgmtNode;

   NoAllocBufferStore* bufStore = App_getMsgBufStore(this->app);
   Node* localNode = App_getLocalNode(this->app);
   const char* localNodeID = Node_getID(localNode);
   NumNodeID localNodeNumID = Node_getNumID(localNode);

   RemoveNodeMsgEx msg;
   char* respBuf;
   NetMessage* respMsg;
   FhgfsOpsErr requestRes;
   //RemoveNodeRespMsg* rmResp; // response value not needed currently
   bool nodeUnregistered = false;

   mgmtNode = NodeStoreEx_referenceFirstNode(this->mgmtNodes);
   if(!mgmtNode)
      return false;

   RemoveNodeMsgEx_initFromNodeData(&msg, localNodeNumID, NODETYPE_Client);

   // connect & communicate
   requestRes = MessagingTk_requestResponse(this->app, mgmtNode,
      (NetMessage*)&msg, NETMSGTYPE_RemoveNodeResp, &respBuf, &respMsg);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // request/response failed
      goto cleanup_request;
   }

   // handle result
   // rmResp = (RemoveNodeRespMsg*)respMsg; // response value not needed currently

   nodeUnregistered = true;


   // cleanup
   NETMESSAGE_FREE(respMsg);
   NoAllocBufferStore_addBuf(bufStore, respBuf);


cleanup_request:
   Node_put(mgmtNode);

   // log deregistration result
   if(nodeUnregistered)
      Logger_log(log, Log_WARNING, logContext, "Node deregistration successful.");
   else
   {
      Logger_log(log, Log_CRITICAL, logContext,
         "Node deregistration failed. Management node offline?");
      Logger_logFormatted(log, Log_CRITICAL, logContext,
         "In case you didn't enable automatic removal, you will have to remove this ClientID "
         "manually from the system: %s", localNodeID);
   }

   return nodeUnregistered;
}


void __InternodeSyncer_downloadAndSyncNodes(InternodeSyncer* this)
{
   Node* mgmtNode;
   Node* localNode;

   NodeList metaNodesList;
   NumNodeIDList addedMetaNodes;
   NumNodeIDList removedMetaNodes;
   NumNodeID rootNodeID = (NumNodeID){0};
   bool rootIsBuddyMirrored;

   NodeList storageNodesList;
   NumNodeIDList addedStorageNodes;
   NumNodeIDList removedStorageNodes;


   mgmtNode = NodeStoreEx_referenceFirstNode(this->mgmtNodes);
   if(!mgmtNode)
      return;

   localNode = App_getLocalNode(this->app);

   // metadata nodes

   NodeList_init(&metaNodesList);
   NumNodeIDList_init(&addedMetaNodes);
   NumNodeIDList_init(&removedMetaNodes);

   if(NodesTk_downloadNodes(this->app, mgmtNode, NODETYPE_Meta, &metaNodesList, &rootNodeID,
      &rootIsBuddyMirrored) )
   {
      const NodeOrGroup rootOwner = rootIsBuddyMirrored
         ? NodeOrGroup_fromGroup(rootNodeID.value)
         : NodeOrGroup_fromNode(rootNodeID);

      NodeStoreEx_syncNodes(this->metaNodes,
         &metaNodesList, &addedMetaNodes, &removedMetaNodes, localNode);
      NodeStoreEx_setRootOwner(this->metaNodes, rootOwner, false);
      __InternodeSyncer_printSyncResults(this, NODETYPE_Meta, &addedMetaNodes, &removedMetaNodes);
   }

   NodeList_uninit(&metaNodesList);
   NumNodeIDList_uninit(&addedMetaNodes);
   NumNodeIDList_uninit(&removedMetaNodes);

   // storage nodes

   NodeList_init(&storageNodesList);
   NumNodeIDList_init(&addedStorageNodes);
   NumNodeIDList_init(&removedStorageNodes);

   if(NodesTk_downloadNodes(this->app, mgmtNode, NODETYPE_Storage, &storageNodesList, NULL, NULL) )
   {
      NodeStoreEx_syncNodes(this->storageNodes,
         &storageNodesList, &addedStorageNodes, &removedStorageNodes, localNode);
      __InternodeSyncer_printSyncResults(this,
         NODETYPE_Storage, &addedStorageNodes, &removedStorageNodes);
   }

   // cleanup

   NodeList_uninit(&storageNodesList);
   NumNodeIDList_uninit(&addedStorageNodes);
   NumNodeIDList_uninit(&removedStorageNodes);

   Node_put(mgmtNode);
}

void __InternodeSyncer_printSyncResults(InternodeSyncer* this, NodeType nodeType,
   NumNodeIDList* addedNodes, NumNodeIDList* removedNodes)
{
   const char* logContext = "Sync";
   Logger* log = App_getLogger(this->app);

   if(NumNodeIDList_length(addedNodes) )
      Logger_logFormatted(log, Log_WARNING, logContext,
         "Nodes added (sync results): %d (Type: %s)",
         (int)NumNodeIDList_length(addedNodes), Node_nodeTypeToStr(nodeType) );

   if(NumNodeIDList_length(removedNodes) )
      Logger_logFormatted(log, Log_WARNING, logContext,
         "Nodes removed (sync results): %d (Type: %s)",
         (int)NumNodeIDList_length(removedNodes), Node_nodeTypeToStr(nodeType) );
}

void __InternodeSyncer_downloadAndSyncTargetMappings(InternodeSyncer* this)
{
   TargetMapper* targetMapper = App_getTargetMapper(this->app);
   Node* mgmtNode;
   bool downloadRes;

   LIST_HEAD(mappings);

   mgmtNode = NodeStoreEx_referenceFirstNode(this->mgmtNodes);
   if(!mgmtNode)
      return;

   downloadRes = NodesTk_downloadTargetMappings(this->app, mgmtNode, &mappings);
   if(downloadRes)
      TargetMapper_syncTargets(targetMapper, /*move*/&mappings);

   // cleanup

   Node_put(mgmtNode);
}


/**
 * note: currently only downloads storage targets, not meta.
 *
 * @param nodeType the node type (NODETYPE_Storage or NODTYPE_Meta) for which the target states
 *                 and buddy groups should be synced
 */
void __InternodeSyncer_updateTargetStatesAndBuddyGroups(InternodeSyncer* this, NodeType nodeType)
{
   const char* logContext = "Update states and mirror groups";
   Logger* log = App_getLogger(this->app);

   NodeStoreEx* mgmtdNodes = App_getMgmtNodes(this->app);
   TargetStateStore* targetStateStore;
   MirrorBuddyGroupMapper* buddyGroupMapper;

   Node* mgmtdNode;
   bool downloadRes;

   LIST_HEAD(buddyGroups); /* struct BuddyGroupMapping */
   LIST_HEAD(states); /* struct TargetStateMapping */

   switch (nodeType)
   {
      case NODETYPE_Storage:
         buddyGroupMapper = App_getStorageBuddyGroupMapper(this->app);
         targetStateStore = App_getTargetStateStore(this->app);
         break;

      case NODETYPE_Meta:
         buddyGroupMapper = App_getMetaBuddyGroupMapper(this->app);
         targetStateStore = App_getMetaStateStore(this->app);
         break;

      default:
         return;
   }

   mgmtdNode = NodeStoreEx_referenceFirstNode(mgmtdNodes);
   if(!mgmtdNode)
      return;

   downloadRes = NodesTk_downloadStatesAndBuddyGroups(this->app, mgmtdNode, nodeType,
      &buddyGroups, &states);

   if(downloadRes)
   {
      TargetStateStore_syncStatesAndGroupsFromLists(targetStateStore, this->app->cfg,
         buddyGroupMapper, &states, &buddyGroups);

      Time_setToNow(&this->lastSuccessfulTargetStatesUpdateT);
      Logger_logFormatted(log, Log_DEBUG, logContext, "%s states synced.",
         (nodeType == NODETYPE_Meta) ? "Metadata node" : "Storage target");
   }
   else
   if( (this->targetOfflineTimeoutMS != 0)
      && (Time_elapsedMS(&this->lastSuccessfulTargetStatesUpdateT) > this->targetOfflineTimeoutMS) )
   {
      bool setStateRes =
         TargetStateStore_setAllStates(targetStateStore, TargetReachabilityState_OFFLINE);

      if(setStateRes)
         Logger_logFormatted(log, Log_WARNING, logContext,
            "%s state sync failed. All %s set to offline.",
            (nodeType == NODETYPE_Meta) ? "Metadata node" : "Storage target",
            (nodeType == NODETYPE_Meta) ? "nodes" : "targets");
   }
   else
   {
      bool setStatesRes =
         TargetStateStore_setAllStates(targetStateStore, TargetReachabilityState_POFFLINE);

      if(setStatesRes)
         Logger_logFormatted(log, Log_WARNING, logContext,
            "%s state sync failed. All %s set to probably-offline.",
            (nodeType == NODETYPE_Meta) ? "Metadata node" : "Storage target",
            (nodeType == NODETYPE_Meta) ? "nodes" : "targets");
   }

   // cleanup

   BEEGFS_KFREE_LIST(&buddyGroups, struct BuddyGroupMapping, _list);
   BEEGFS_KFREE_LIST(&states, struct TargetStateMapping, _list);

   Node_put(mgmtdNode);
}


/**
 * Try to close all delayedClose files.
 */
void __InternodeSyncer_delayedCloseComm(InternodeSyncer* this)
{
   PointerListIter iter;

   Mutex_lock(&this->delayedCloseMutex); // L O C K

   PointerListIter_init(&iter, &this->delayedCloseQueue);

   while(!PointerListIter_end(&iter) && !Thread_getSelfTerminate( (Thread*)this) )
   {
      DelayedCloseEntry* currentClose = PointerListIter_value(&iter);

      EntryInfo* entryInfo;
      RemotingIOInfo ioInfo;
      FhgfsOpsErr closeRes;

      __InternodeSyncer_delayedClosePrepareRemoting(this, currentClose, &entryInfo, &ioInfo);

      // note: unlock, so that more entries can be added to the queue during remoting
      Mutex_unlock(&this->delayedCloseMutex); // U N L O C K

      closeRes = FhgfsOpsRemoting_closefileEx(entryInfo, &ioInfo, false,
            currentClose->hasEvent ? &currentClose->event : NULL);

      Mutex_lock(&this->delayedCloseMutex); // R E L O C K

      if(closeRes == FhgfsOpsErr_COMMUNICATION)
      { // comm error => we will try again later
         PointerListIter_next(&iter);
      }
      else
      { /* anything other than communication error means our job is done and we can delete the
           entry */
         __InternodeSyncer_delayedCloseFreeEntry(this, currentClose);

         iter = PointerListIter_remove(&iter);
      }
   }


   Mutex_unlock(&this->delayedCloseMutex); // U N L O C K
}

/**
 * Try to unlock all delayedEntryUnlock files.
 */
void __InternodeSyncer_delayedEntryUnlockComm(InternodeSyncer* this)
{
   PointerListIter iter;

   Mutex_lock(&this->delayedEntryUnlockMutex); // L O C K

   PointerListIter_init(&iter, &this->delayedEntryUnlockQueue);

   while(!PointerListIter_end(&iter) && !Thread_getSelfTerminate( (Thread*)this) )
   {
      DelayedEntryUnlockEntry* currentUnlock = PointerListIter_value(&iter);

      EntryInfo* entryInfo;
      RemotingIOInfo ioInfo;
      FhgfsOpsErr unlockRes;

      __InternodeSyncer_delayedEntryUnlockPrepareRemoting(this, currentUnlock, &entryInfo, &ioInfo);

      // note: unlock, so that more entries can be added to the queue during remoting
      Mutex_unlock(&this->delayedEntryUnlockMutex); // U N L O C K

      unlockRes = FhgfsOpsRemoting_flockEntryEx(entryInfo, NULL, this->app, ioInfo.fileHandleID,
         currentUnlock->clientFD, 0, ENTRYLOCKTYPE_CANCEL, false);

      Mutex_lock(&this->delayedEntryUnlockMutex); // R E L O C K

      if(unlockRes == FhgfsOpsErr_COMMUNICATION)
      { // comm error => we will try again later
         PointerListIter_next(&iter);
      }
      else
      { /* anything other than communication error means our job is done and we can delete the
           entry */
         __InternodeSyncer_delayedEntryUnlockFreeEntry(this, currentUnlock);

         iter = PointerListIter_remove(&iter);
      }
   }


   Mutex_unlock(&this->delayedEntryUnlockMutex); // U N L O C K
}

/**
 * Try to unlock all delayedRangeUnlock files.
 */
void __InternodeSyncer_delayedRangeUnlockComm(InternodeSyncer* this)
{
   PointerListIter iter;

   Mutex_lock(&this->delayedRangeUnlockMutex); // L O C K

   PointerListIter_init(&iter, &this->delayedRangeUnlockQueue);

   while(!PointerListIter_end(&iter) && !Thread_getSelfTerminate( (Thread*)this) )
   {
      DelayedRangeUnlockEntry* currentUnlock = PointerListIter_value(&iter);

      EntryInfo* entryInfo;
      RemotingIOInfo ioInfo;
      FhgfsOpsErr unlockRes;

      __InternodeSyncer_delayedRangeUnlockPrepareRemoting(this, currentUnlock, &entryInfo, &ioInfo);

      // note: unlock, so that more entries can be added to the queue during remoting
      Mutex_unlock(&this->delayedRangeUnlockMutex); // U N L O C K

      unlockRes = FhgfsOpsRemoting_flockRangeEx(entryInfo, NULL, ioInfo.app, ioInfo.fileHandleID,
         currentUnlock->ownerPID, ENTRYLOCKTYPE_CANCEL, 0, ~0ULL, false);

      Mutex_lock(&this->delayedRangeUnlockMutex); // R E L O C K

      if(unlockRes == FhgfsOpsErr_COMMUNICATION)
      { // comm error => we will try again later
         PointerListIter_next(&iter);
      }
      else
      { /* anything other than communication error means our job is done and we can delete the
           entry */
         __InternodeSyncer_delayedRangeUnlockFreeEntry(this, currentUnlock);

         iter = PointerListIter_remove(&iter);
      }
   }


   Mutex_unlock(&this->delayedRangeUnlockMutex); // U N L O C K
}


/**
 * Prepare remoting args for delayed close.
 *
 * Note: This uses only references to the closeEntry, so no cleanup call required.
 */
void __InternodeSyncer_delayedClosePrepareRemoting(InternodeSyncer* this,
   DelayedCloseEntry* closeEntry, EntryInfo** outEntryInfo, RemotingIOInfo* outIOInfo)
{
   *outEntryInfo = &closeEntry->entryInfo;

   outIOInfo->app = this->app;
   outIOInfo->fileHandleID = closeEntry->fileHandleID;
   outIOInfo->pattern = NULL;
   outIOInfo->accessFlags = closeEntry->accessFlags;
   outIOInfo->needsAppendLockCleanup = &closeEntry->needsAppendLockCleanup;
   outIOInfo->maxUsedTargetIndex = &closeEntry->maxUsedTargetIndex;
   outIOInfo->firstWriteDone = NULL;
   outIOInfo->userID = 0;
   outIOInfo->groupID = 0;
#ifdef BEEGFS_NVFS
   outIOInfo->nvfs = false;
#endif
}

/**
 * Prepare remoting args for delayed entry unlock.
 *
 * Note: This uses only references to the unlockEntry, so no cleanup call required.
 */
void __InternodeSyncer_delayedEntryUnlockPrepareRemoting(InternodeSyncer* this,
   DelayedEntryUnlockEntry* unlockEntry, EntryInfo** outEntryInfo, RemotingIOInfo* outIOInfo)
{
   *outEntryInfo = &unlockEntry->entryInfo;

   outIOInfo->app = this->app;
   outIOInfo->fileHandleID = unlockEntry->fileHandleID;
   outIOInfo->pattern = NULL;
   outIOInfo->accessFlags = 0;
   outIOInfo->maxUsedTargetIndex = NULL;
   outIOInfo->firstWriteDone = NULL;
   outIOInfo->userID = 0;
   outIOInfo->groupID = 0;
#ifdef BEEGFS_NVFS
   outIOInfo->nvfs = false;
#endif
}

/**
 * Prepare remoting args for delayed range unlock.
 *
 * Note: This uses only references to the unlockEntry, so no cleanup call required.
 */
void __InternodeSyncer_delayedRangeUnlockPrepareRemoting(InternodeSyncer* this,
   DelayedRangeUnlockEntry* unlockEntry, EntryInfo** outEntryInfo, RemotingIOInfo* outIOInfo)
{
   *outEntryInfo = &unlockEntry->entryInfo;

   outIOInfo->app = this->app;
   outIOInfo->fileHandleID = unlockEntry->fileHandleID;
   outIOInfo->pattern = NULL;
   outIOInfo->accessFlags = 0;
   outIOInfo->maxUsedTargetIndex = NULL;
   outIOInfo->firstWriteDone = NULL;
   outIOInfo->userID = 0;
   outIOInfo->groupID = 0;
#ifdef BEEGFS_NVFS
   outIOInfo->nvfs = false;
#endif
}

/**
 * Frees/uninits all sub-fields and kfrees the closeEntry itself (but does not remove it from the
 * queue).
 */
void __InternodeSyncer_delayedCloseFreeEntry(InternodeSyncer* this, DelayedCloseEntry* closeEntry)
{
   EntryInfo_uninit(&closeEntry->entryInfo);
   if (closeEntry->hasEvent)
      FileEvent_uninit(&closeEntry->event);

   kfree(closeEntry->fileHandleID);

   AtomicInt_uninit(&closeEntry->maxUsedTargetIndex);

   kfree(closeEntry);
}

/**
 * Frees/uninits all sub-fields and kfrees the unlockEntry itself (but does not remove it from the
 * queue).
 */
void __InternodeSyncer_delayedEntryUnlockFreeEntry(InternodeSyncer* this,
   DelayedEntryUnlockEntry* unlockEntry)
{
   EntryInfo_uninit(&unlockEntry->entryInfo);

   kfree(unlockEntry->fileHandleID);

   kfree(unlockEntry);
}

/**
 * Frees/uninits all sub-fields and kfrees the unlockEntry itself (but does not remove it from the
 * queue).
 */
void __InternodeSyncer_delayedRangeUnlockFreeEntry(InternodeSyncer* this,
   DelayedRangeUnlockEntry* unlockEntry)
{
   EntryInfo_uninit(&unlockEntry->entryInfo);

   kfree(unlockEntry->fileHandleID);

   kfree(unlockEntry);
}

/**
 * Add an entry that could not be closed on the server due to communication error and should be
 * retried again later.
 *
 * @param entryInfo will be copied
 * @param ioInfo will be copied
 */
void InternodeSyncer_delayedCloseAdd(InternodeSyncer* this, const EntryInfo* entryInfo,
   const RemotingIOInfo* ioInfo, struct FileEvent* event)
{
   DelayedCloseEntry* newClose = (DelayedCloseEntry*)os_kmalloc(sizeof(*newClose) );

   Time_init(&newClose->ageT);

   EntryInfo_dup(entryInfo, &newClose->entryInfo);

   newClose->fileHandleID = StringTk_strDup(ioInfo->fileHandleID);
   newClose->accessFlags = ioInfo->accessFlags;

   newClose->needsAppendLockCleanup =
      (ioInfo->needsAppendLockCleanup && *ioInfo->needsAppendLockCleanup);

   AtomicInt_init(&newClose->maxUsedTargetIndex, AtomicInt_read(ioInfo->maxUsedTargetIndex) );

   newClose->hasEvent = event != NULL;
   if (event)
      newClose->event = *event;

   Mutex_lock(&this->delayedCloseMutex); // L O C K

   PointerList_append(&this->delayedCloseQueue, newClose);

   Mutex_unlock(&this->delayedCloseMutex); // U N L O C K
}

/**
 * Add an entry that could not be unlocked on the server due to communication error and should be
 * retried again later.
 *
 * @param entryInfo will be copied
 * @param ioInfo will be copied
 */
void InternodeSyncer_delayedEntryUnlockAdd(InternodeSyncer* this, const EntryInfo* entryInfo,
   const RemotingIOInfo* ioInfo, int64_t clientFD)
{
   DelayedEntryUnlockEntry* newUnlock = (DelayedEntryUnlockEntry*)os_kmalloc(sizeof(*newUnlock) );

   Time_init(&newUnlock->ageT);

   EntryInfo_dup(entryInfo, &newUnlock->entryInfo);

   newUnlock->fileHandleID = StringTk_strDup(ioInfo->fileHandleID);
   newUnlock->clientFD = clientFD;

   Mutex_lock(&this->delayedEntryUnlockMutex); // L O C K

   PointerList_append(&this->delayedEntryUnlockQueue, newUnlock);

   Mutex_unlock(&this->delayedEntryUnlockMutex); // U N L O C K
}

/**
 * Add an entry that could not be unlocked on the server due to communication error and should be
 * retried again later.
 *
 * @param entryInfo will be copied
 * @param ioInfo will be copied
 */
void InternodeSyncer_delayedRangeUnlockAdd(InternodeSyncer* this, const EntryInfo* entryInfo,
   const RemotingIOInfo* ioInfo, int ownerPID)
{
   DelayedRangeUnlockEntry* newUnlock = (DelayedRangeUnlockEntry*)os_kmalloc(sizeof(*newUnlock) );

   Time_init(&newUnlock->ageT);

   EntryInfo_dup(entryInfo, &newUnlock->entryInfo);

   newUnlock->fileHandleID = StringTk_strDup(ioInfo->fileHandleID);
   newUnlock->ownerPID = ownerPID;

   Mutex_lock(&this->delayedRangeUnlockMutex); // L O C K

   PointerList_append(&this->delayedRangeUnlockQueue, newUnlock);

   Mutex_unlock(&this->delayedRangeUnlockMutex); // U N L O C K
}

size_t InternodeSyncer_getDelayedCloseQueueSize(InternodeSyncer* this)
{
   size_t retVal;

   Mutex_lock(&this->delayedCloseMutex); // L O C K

   retVal = PointerList_length(&this->delayedCloseQueue);

   Mutex_unlock(&this->delayedCloseMutex); // U N L O C K

   return retVal;
}

size_t InternodeSyncer_getDelayedEntryUnlockQueueSize(InternodeSyncer* this)
{
   size_t retVal;

   Mutex_lock(&this->delayedEntryUnlockMutex); // L O C K

   retVal = PointerList_length(&this->delayedEntryUnlockQueue);

   Mutex_unlock(&this->delayedEntryUnlockMutex); // U N L O C K

   return retVal;
}

size_t InternodeSyncer_getDelayedRangeUnlockQueueSize(InternodeSyncer* this)
{
   size_t retVal;

   Mutex_lock(&this->delayedRangeUnlockMutex); // L O C K

   retVal = PointerList_length(&this->delayedRangeUnlockQueue);

   Mutex_unlock(&this->delayedRangeUnlockMutex); // U N L O C K

   return retVal;
}

/**
 * Drop/reset idle conns from all server stores.
 */
void __InternodeSyncer_dropIdleConns(InternodeSyncer* this)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "Idle disconnect";

   unsigned numDroppedConns = 0;

   numDroppedConns += __InternodeSyncer_dropIdleConnsByStore(this, App_getMgmtNodes(this->app) );
   numDroppedConns += __InternodeSyncer_dropIdleConnsByStore(this, App_getMetaNodes(this->app) );
   numDroppedConns += __InternodeSyncer_dropIdleConnsByStore(this, App_getStorageNodes(this->app) );

   if(numDroppedConns)
   {
      Logger_logFormatted(log, Log_DEBUG, logContext, "Dropped idle connections: %u",
         numDroppedConns);
   }
}

/**
 * Walk over all nodes in the given store and drop/reset idle connections.
 *
 * @return number of dropped connections
 */
unsigned __InternodeSyncer_dropIdleConnsByStore(InternodeSyncer* this, NodeStoreEx* nodes)
{
   unsigned numDroppedConns = 0;

   Node* node = NodeStoreEx_referenceFirstNode(nodes);
   while(node)
   {
      NodeConnPool* connPool = Node_getConnPool(node);

      numDroppedConns += NodeConnPool_disconnectAndResetIdleStreams(connPool);

      node = NodeStoreEx_referenceNextNodeAndReleaseOld(nodes, node); // iterate to next node
   }

   return numDroppedConns;
}

