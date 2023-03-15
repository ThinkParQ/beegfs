#include <app/config/Config.h>
#include <app/App.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/Time.h>
#include <common/nodes/NodeStore.h>
#include <common/nodes/TargetCapacityPools.h>
#include <program/Program.h>
#include "InternodeSyncer.h"

#include <mutex>

#include <boost/lexical_cast.hpp>

InternodeSyncer::InternodeSyncer() :
   PThread("XNodeSync"),
   log("XNodeSync"),
   serversDownloaded(false)
{
}

InternodeSyncer::~InternodeSyncer()
{
}


void InternodeSyncer::run()
{
   try
   {
      registerSignalHandler();

      // download all nodes,mappings,states and buddy groups
      NumNodeIDList addedStorageNodes;
      NumNodeIDList removedStorageNodes;
      NumNodeIDList addedMetaNodes;
      NumNodeIDList removedMetaNodes;
      bool syncRes = downloadAndSyncNodes(addedStorageNodes, removedStorageNodes, addedMetaNodes,
         removedMetaNodes);
      if (!syncRes)
      {
         log.logErr("Error downloading nodes from mgmtd.");
         Program::getApp()->abort();
         return;
      }

      syncRes = downloadAndSyncTargetMappings();
      if (!syncRes)
      {
         log.logErr("Error downloading target mappings from mgmtd.");
         Program::getApp()->abort();
         return;
      }

      originalTargetMap = Program::getApp()->getTargetMapper()->getMapping();

      syncRes = downloadAndSyncTargetStates();
      if (!syncRes)
      {
         log.logErr("Error downloading target states from mgmtd.");
         Program::getApp()->abort();
         return;
      }

      syncRes = downloadAndSyncMirrorBuddyGroups();
      if ( !syncRes )
      {
         log.logErr("Error downloading mirror buddy groups from mgmtd.");
         Program::getApp()->abort();
         return;
      }

      Program::getApp()->getMirrorBuddyGroupMapper()->getMirrorBuddyGroups(
         originalMirrorBuddyGroupMap);

      syncRes = downloadAndSyncMetaMirrorBuddyGroups();
      if ( !syncRes )
      {
         log.logErr("Error downloading metadata mirror buddy groups from mgmtd.");
         Program::getApp()->abort();
         return;
      }

      Program::getApp()->getMetaMirrorBuddyGroupMapper()->getMirrorBuddyGroups(
         originalMetaMirrorBuddyGroupMap);

      {
         std::lock_guard<Mutex> lock(serversDownloadedMutex);
         serversDownloaded = true;
         serversDownloadedCondition.signal();
      }

      syncLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

void InternodeSyncer::syncLoop()
{
   const int sleepIntervalMS = 3*1000; // 3sec
   const unsigned downloadNodesAndStatesIntervalMS = 30000; // 30 sec
   const unsigned checkNetworkIntervalMS = 60*1000; // 1 minute

   Time lastDownloadNodesAndStatesT;
   Time lastCheckNetworkT;

   while(!waitForSelfTerminateOrder(sleepIntervalMS) )
   {
      // download & sync nodes
      if (lastDownloadNodesAndStatesT.elapsedMS() > downloadNodesAndStatesIntervalMS)
      {
         NumNodeIDList addedStorageNodes;
         NumNodeIDList removedStorageNodes;
         NumNodeIDList addedMetaNodes;
         NumNodeIDList removedMetaNodes;
         bool syncRes = downloadAndSyncNodes(addedStorageNodes, removedStorageNodes, addedMetaNodes,
            removedMetaNodes);

         if (!syncRes)
         {
            log.logErr("Error downloading nodes from mgmtd.");
            Program::getApp()->abort();
            break;
         }

         handleNodeChanges(NODETYPE_Meta, addedMetaNodes, removedMetaNodes);

         handleNodeChanges(NODETYPE_Storage, addedStorageNodes, removedStorageNodes);

         syncRes = downloadAndSyncTargetMappings();
         if (!syncRes)
         {
            log.logErr("Error downloading target mappings from mgmtd.");
            Program::getApp()->abort();
            break;
         }

         handleTargetMappingChanges();

         syncRes = downloadAndSyncTargetStates();
         if (!syncRes)
         {
            log.logErr("Error downloading target states from mgmtd.");
            Program::getApp()->abort();
            break;
         }

         syncRes = downloadAndSyncMirrorBuddyGroups();
         if ( !syncRes )
         {
            log.logErr("Error downloading mirror buddy groups from mgmtd.");
            Program::getApp()->abort();
            break;
         }

         handleBuddyGroupChanges();

         lastDownloadNodesAndStatesT.setToNow();
      }

      bool checkNetworkForced = getAndResetForceCheckNetwork();

      if( checkNetworkForced ||
         (lastCheckNetworkT.elapsedMS() > checkNetworkIntervalMS))
      {
         checkNetwork();
         lastCheckNetworkT.setToNow();
      }

   }
}

/**
 * @return false on error.
 */
bool InternodeSyncer::downloadAndSyncTargetStates()
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   TargetStateStore* targetStateStore = app->getTargetStateStore();

   auto node = mgmtNodes->referenceFirstNode();
   if(!node)
      return false;

   UInt16List targetIDs;
   UInt8List reachabilityStates;
   UInt8List consistencyStates;

   bool downloadRes = NodesTk::downloadTargetStates(*node, NODETYPE_Storage,
      &targetIDs, &reachabilityStates, &consistencyStates, false);

   if(downloadRes)
      targetStateStore->syncStatesFromLists(targetIDs, reachabilityStates,
         consistencyStates);

   return downloadRes;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncNodes(NumNodeIDList& addedStorageNodes,
   NumNodeIDList& removedStorageNodes, NumNodeIDList& addedMetaNodes,
   NumNodeIDList& removedMetaNodes)
{
   const char* logContext = "Nodes sync";

   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   Node& localNode = app->getLocalNode();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
      return false;

   { // storage nodes
      std::vector<NodeHandle> storageNodesList;

      bool storageRes =
         NodesTk::downloadNodes(*mgmtNode, NODETYPE_Storage, storageNodesList, false);
      if(!storageRes)
         return false;

      storageNodes->syncNodes(storageNodesList, &addedStorageNodes, &removedStorageNodes,
         &localNode);
      printSyncNodesResults(NODETYPE_Storage, &addedStorageNodes, &removedStorageNodes);
   }

   { // metadata nodes
      std::vector<NodeHandle> metaNodesList;
      NumNodeID rootNodeID;
      bool rootIsBuddyMirrored;

      bool metaRes =
         NodesTk::downloadNodes(*mgmtNode, NODETYPE_Meta, metaNodesList, false, &rootNodeID,
            &rootIsBuddyMirrored);
      if(!metaRes)
         return false;

      metaNodes->syncNodes(metaNodesList, &addedMetaNodes, &removedMetaNodes, &localNode);

      if (app->getMetaRoot().setIfDefault(rootNodeID, rootIsBuddyMirrored))
      {
         LogContext(logContext).log(Log_CRITICAL,
            "Root NodeID (from sync results): " + rootNodeID.str() );
      }

      printSyncNodesResults(NODETYPE_Meta, &addedMetaNodes, &removedMetaNodes);
   }

   return true;
}

void InternodeSyncer::printSyncNodesResults(NodeType nodeType, NumNodeIDList* addedNodes,
   NumNodeIDList* removedNodes)
{
   const char* logContext = "Sync results";

   if (!addedNodes->empty())
      LogContext(logContext).log(Log_WARNING, std::string("Nodes added: ") +
         StringTk::uintToStr(addedNodes->size() ) +
         " (Type: " + boost::lexical_cast<std::string>(nodeType) + ")");

   if (!removedNodes->empty())
      LogContext(logContext).log(Log_WARNING, std::string("Nodes removed: ") +
         StringTk::uintToStr(removedNodes->size() ) +
         " (Type: " + boost::lexical_cast<std::string>(nodeType) + ")");
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncTargetMappings()
{
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   TargetMapper* targetMapper = app->getTargetMapper();

   bool retVal = true;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   auto mappings = NodesTk::downloadTargetMappings(*mgmtNode, false);
   if (mappings.first)
      targetMapper->syncTargets(std::move(mappings.second));
   else
      retVal = false;

   return retVal;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncMirrorBuddyGroups()
{
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();

   bool retVal = true;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;

   bool downloadRes = NodesTk::downloadMirrorBuddyGroups(*mgmtNode, NODETYPE_Storage,
         &buddyGroupIDs, &primaryTargetIDs, &secondaryTargetIDs, false);

   if(downloadRes)
      buddyGroupMapper->syncGroupsFromLists(buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs,
         NumNodeID() );
   else
      retVal = false;

   return retVal;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncMetaMirrorBuddyGroups()
{
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

   bool retVal = true;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;

   bool downloadRes = NodesTk::downloadMirrorBuddyGroups(*mgmtNode, NODETYPE_Meta, &buddyGroupIDs,
      &primaryTargetIDs, &secondaryTargetIDs, false);

   if(downloadRes)
      buddyGroupMapper->syncGroupsFromLists(buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs,
         NumNodeID() );
   else
      retVal = false;

   return retVal;
}

void InternodeSyncer::handleNodeChanges(NodeType nodeType, NumNodeIDList& addedNodes,
   NumNodeIDList& removedNodes)
{
   const char* logContext = "handleNodeChanges";

   if (!addedNodes.empty())
      LogContext(logContext).log(Log_WARNING,
         std::string("Nodes added while beegfs-fsck was running: ")
            + StringTk::uintToStr(addedNodes.size())
            + " (Type: " + boost::lexical_cast<std::string>(nodeType) + ")");

   if (!removedNodes.empty())
   {
      // removed nodes must lead to fsck stoppage
      LogContext(logContext).log(Log_WARNING,
         std::string("Nodes removed while beegfs-fsck was running: ")
            + StringTk::uintToStr(removedNodes.size())
            + " (Type: " + boost::lexical_cast<std::string>(nodeType) + ")");

      Program::getApp()->abort();
   }
}

void InternodeSyncer::handleTargetMappingChanges()
{
   const char* logContext = "handleTargetMappingChanges";

   TargetMap newTargetMap = Program::getApp()->getTargetMapper()->getMapping();

   for ( TargetMapIter originalMapIter = originalTargetMap.begin();
      originalMapIter != originalTargetMap.end(); originalMapIter++ )
   {
      uint16_t targetID = originalMapIter->first;
      NumNodeID oldNodeID = originalMapIter->second;

      TargetMapIter newMapIter = newTargetMap.find(targetID);

      if ( newMapIter == newTargetMap.end() )
      {
         LogContext(logContext).log(Log_WARNING,
            "Target removed while beegfs-fsck was running; beegfs-fsck can't continue; targetID: "
               + StringTk::uintToStr(targetID));

         Program::getApp()->abort();
      }
      else
      {
         NumNodeID newNodeID = newMapIter->second;
         if ( oldNodeID != newNodeID )
         {
            LogContext(logContext).log(Log_WARNING,
               "Target re-mapped while beegfs-fsck was running; beegfs-fsck can't continue; "
                  "targetID: " + StringTk::uintToStr(targetID));

            Program::getApp()->abort();
         }
      }
   }
}

void InternodeSyncer::handleBuddyGroupChanges()
{
   const char* logContext = "handleBuddyGroupChanges";

   MirrorBuddyGroupMap newMirrorBuddyGroupMap;
   Program::getApp()->getMirrorBuddyGroupMapper()->getMirrorBuddyGroups(newMirrorBuddyGroupMap);

   for ( MirrorBuddyGroupMapIter originalMapIter = originalMirrorBuddyGroupMap.begin();
      originalMapIter != originalMirrorBuddyGroupMap.end(); originalMapIter++ )
   {
      uint16_t buddyGroupID = originalMapIter->first;
      MirrorBuddyGroup oldBuddyGroup = originalMapIter->second;

      MirrorBuddyGroupMapIter newMapIter = newMirrorBuddyGroupMap.find(buddyGroupID);

      if ( newMapIter == newMirrorBuddyGroupMap.end() )
      {
         LogContext(logContext).log(Log_WARNING,
            "Mirror buddy group removed while beegfs-fsck was running; beegfs-fsck can't continue; "
            "groupID: " + StringTk::uintToStr(buddyGroupID));

         Program::getApp()->abort();
      }
      else
      {
         MirrorBuddyGroup newBuddyGroup = newMapIter->second;
         if ( oldBuddyGroup.firstTargetID != newBuddyGroup.firstTargetID )
         {
            LogContext(logContext).log(Log_WARNING,
               "Primary of mirror buddy group changed while beegfs-fsck was running; beegfs-fsck "
               "can't continue; groupID: " + StringTk::uintToStr(buddyGroupID));

            Program::getApp()->abort();
         }
      }
   }
}

/**
 * Blocks until list of servers has been downloaded from management node
 */
void InternodeSyncer::waitForServers()
{
   std::lock_guard<Mutex> lock(serversDownloadedMutex);
   while (!serversDownloaded)
      serversDownloadedCondition.wait(&serversDownloadedMutex);
}

/**
 * Inspect the available and allowed network interfaces for any changes.
 */
bool InternodeSyncer::checkNetwork()
{
   App* app = Program::getApp();
   NicAddressList newLocalNicList;
   bool res = false;

   app->findAllowedInterfaces(newLocalNicList);
   app->findAllowedRDMAInterfaces(newLocalNicList);
   if (!std::equal(newLocalNicList.begin(), newLocalNicList.end(), app->getLocalNicList().begin()))
   {
      log.log(Log_NOTICE, "checkNetwork: local interfaces have changed");
      app->updateLocalNicList(newLocalNicList);
      res = true;
   }

   return res;
}
