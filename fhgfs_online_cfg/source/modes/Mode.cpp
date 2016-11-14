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

   const std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   const unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();
   const int mgmtTimeoutMS = 2500;

   if (mgmtHost.empty())
      throw ComponentInitException("Management node not configured");

   // check mgmt node
   if(!NodesTk::waitForMgmtHeartbeat(NULL, dgramLis, mgmtNodes, mgmtHost, mgmtPortUDP,
      mgmtTimeoutMS))
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

      NodesTk::applyLocalNicCapsToList(app->getLocalNode(), metaNodesList);
      NodesTk::moveNodesFromListToStore(metaNodesList, metaNodes);
      metaNodes->setRootNodeNumID(rootNodeID, false, rootIsBuddyMirrored);
   }

   { // download storage nodes
      std::vector<NodeHandle> storageNodesList;

      if(!NodesTk::downloadNodes(*mgmtNode, NODETYPE_Storage, storageNodesList, false, NULL))
         throw ComponentInitException("Storage nodes download failed");

      NodesTk::applyLocalNicCapsToList(app->getLocalNode(), storageNodesList);
      NodesTk::moveNodesFromListToStore(storageNodesList, storageNodes);
   }

   { // download client nodes
      std::vector<NodeHandle> clientNodesList;

      if(!NodesTk::downloadNodes(*mgmtNode, NODETYPE_Client, clientNodesList, false, NULL))
         throw ComponentInitException("Clients download failed");

      NodesTk::applyLocalNicCapsToList(app->getLocalNode(), clientNodesList);
      NodesTk::moveNodesFromListToStore(clientNodesList, clientNodes);
   }

   { // download target mappings
      UInt16List targetIDs;
      NumNodeIDList nodeIDs;

      if (!NodesTk::downloadTargetMappings(*mgmtNode, &targetIDs, &nodeIDs, false))
         throw ComponentInitException("Target mappings download failed");

      targetMapper->syncTargetsFromLists(targetIDs, nodeIDs);
   }

   { // download storage buddy groups and target states
      UInt16List buddyGroupIDs;
      UInt16List primaryTargetIDs;
      UInt16List secondaryTargetIDs;
      UInt16List targetIDs;
      UInt8List reachabilityStates;
      UInt8List consistencyStates;

      if (!NodesTk::downloadStatesAndBuddyGroups(*mgmtNode, NODETYPE_Storage, &buddyGroupIDs,
            &primaryTargetIDs, &secondaryTargetIDs, &targetIDs, &reachabilityStates,
            &consistencyStates, false))
         throw ComponentInitException("Storage buddy groups and target states download failed");

      storageTargetStateStore->syncStatesAndGroupsFromLists(storageBuddyGroupMapper, targetIDs,
         reachabilityStates, consistencyStates, buddyGroupIDs, primaryTargetIDs,
         secondaryTargetIDs, app->getLocalNode().getNumID());
   }

   { // download meta buddy groups and target states
      UInt16List buddyGroupIDs;
      UInt16List primaryTargetIDs;
      UInt16List secondaryTargetIDs;
      UInt16List targetIDs;
      UInt8List reachabilityStates;
      UInt8List consistencyStates;

      if(!NodesTk::downloadStatesAndBuddyGroups(*mgmtNode, NODETYPE_Meta, &buddyGroupIDs,
            &primaryTargetIDs, &secondaryTargetIDs, &targetIDs, &reachabilityStates,
            &consistencyStates, false))
         throw ComponentInitException("Meta buddy groups and target states download failed");

      metaTargetStateStore->syncStatesAndGroupsFromLists(metaMirrorBuddyGroupMapper, targetIDs,
         reachabilityStates, consistencyStates, buddyGroupIDs, primaryTargetIDs,
         secondaryTargetIDs, app->getLocalNode().getNumID());
   }
}
