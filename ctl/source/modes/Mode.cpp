#include <common/components/ComponentInitException.h>
#include <common/toolkit/NodesTk.h>
#include <program/Program.h>
#include "Mode.h"

Mode::Mode(bool skipInit)
{
   if (!skipInit)
      initializeCommonObjects();
}

/*
 * download information from mgtmd and initialize the following objects:
 *
 * App::mgmtNodes
 * App::metaNodes
 * App::storageNodes
 * App::targetMapper
 * App::targetStateStore
 * App::metaTargetStateStore
 * App::mirrorBuddyGroupMapper
 * App::metaMirrorBuddyGroupMapper
 * App::storagePoolStore
 */
void Mode::initializeCommonObjects()
{
   App* app = Program::getApp();
   AbstractDatagramListener* dgramLis = app->getDatagramListener();

   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   NodeStoreClients* clientNodes = app->getClientNodes();
   TargetMapper* targetMapper = app->getTargetMapper();
   TargetStateStore* storageTargetStateStore = app->getTargetStateStore();
   TargetStateStore* metaTargetStateStore = app->getMetaTargetStateStore();
   MirrorBuddyGroupMapper* storageBuddyGroupMapper = app->getMirrorBuddyGroupMapper();
   MirrorBuddyGroupMapper* metaMirrorBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();
   StoragePoolStore* storagePoolStore = app->getStoragePoolStore();

   const std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   const unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();
   const unsigned mgmtTimeoutMS = 2500;
   const unsigned mgmtNameResolutionRetries = 3;

   if (mgmtHost.empty())
      throw ComponentInitException("Management node not configured");

   // check mgmt node
   if(!NodesTk::waitForMgmtHeartbeat(NULL, dgramLis, mgmtNodes, mgmtHost, mgmtPortUDP,
            mgmtTimeoutMS, mgmtNameResolutionRetries))
   {
      throw ComponentInitException("Management node communication failed: " + mgmtHost);
   }

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   { // download meta nodes
      std::vector<NodeHandle> metaNodesList;
      NumNodeID rootNodeID;
      bool rootIsBuddyMirrored;

      if(!NodesTk::downloadNodes(*mgmtNode, NODETYPE_Meta, metaNodesList, false, &rootNodeID,
            &rootIsBuddyMirrored))
         throw ComponentInitException("Metadata nodes download failed");

      NodesTk::applyLocalNicListToList(app->getLocalNode(), metaNodesList);
      NodesTk::moveNodesFromListToStore(metaNodesList, metaNodes);
      app->getMetaRoot().setIfDefault(rootNodeID, rootIsBuddyMirrored);
   }

   { // download storage nodes
      std::vector<NodeHandle> storageNodesList;

      if(!NodesTk::downloadNodes(*mgmtNode, NODETYPE_Storage, storageNodesList, false, NULL))
         throw ComponentInitException("Storage nodes download failed");

      NodesTk::applyLocalNicListToList(app->getLocalNode(), storageNodesList);
      NodesTk::moveNodesFromListToStore(storageNodesList, storageNodes);
   }

   { // download client nodes
      std::vector<NodeHandle> clientNodesList;

      if(!NodesTk::downloadNodes(*mgmtNode, NODETYPE_Client, clientNodesList, false, NULL))
         throw ComponentInitException("Clients download failed");

      NodesTk::applyLocalNicListToList(app->getLocalNode(), clientNodesList);
      NodesTk::moveNodesFromListToStore(clientNodesList, clientNodes);
   }

   { // download target mappings
      auto mappings = NodesTk::downloadTargetMappings(*mgmtNode, false);
      if (!mappings.first)
         throw ComponentInitException("Target mappings download failed");

      targetMapper->syncTargets(std::move(mappings.second));
   }

   { // download storage buddy groups and target states
      MirrorBuddyGroupMap buddyGroups;
      TargetStateMap states;

      if (!NodesTk::downloadStatesAndBuddyGroups(*mgmtNode, NODETYPE_Storage, buddyGroups,
            states, false))
         throw ComponentInitException("Storage buddy groups and target states download failed");

      storageTargetStateStore->syncStatesAndGroups(storageBuddyGroupMapper,
         states, std::move(buddyGroups), app->getLocalNode().getNumID());
   }

   { // download meta buddy groups and target states
      MirrorBuddyGroupMap buddyGroups;
      TargetStateMap states;

      if(!NodesTk::downloadStatesAndBuddyGroups(*mgmtNode, NODETYPE_Meta, buddyGroups,
               states, false))
         throw ComponentInitException("Meta buddy groups and target states download failed");

      metaTargetStateStore->syncStatesAndGroups(metaMirrorBuddyGroupMapper,
         states, std::move(buddyGroups), app->getLocalNode().getNumID());
   }

   { // download storage pools
      StoragePoolPtrVec storagePools;

      if (!NodesTk::downloadStoragePools(*mgmtNode, storagePools, false))
         throw ComponentInitException("Storage pools download failed");

      storagePoolStore->syncFromVector(storagePools);
   }
}
