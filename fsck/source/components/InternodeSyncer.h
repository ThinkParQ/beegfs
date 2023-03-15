#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/threading/PThread.h>
#include <common/Common.h>

#include <mutex>

class InternodeSyncer : public PThread
{
   public:
      InternodeSyncer();
      virtual ~InternodeSyncer();

      bool downloadAndSyncNodes(NumNodeIDList& addedStorageNodes,
         NumNodeIDList& removedStorageNodes, NumNodeIDList& addedMetaNodes,
         NumNodeIDList& removedMetaNodes);
      bool downloadAndSyncTargetMappings();
      bool downloadAndSyncMirrorBuddyGroups();
      bool downloadAndSyncMetaMirrorBuddyGroups();
      bool downloadAndSyncTargetStates();

   private:
      LogContext log;
      Mutex forceNodesAndTargetStatesUpdateMutex;
      bool forceNodesAndTargetStatesUpdate;
      Mutex forceCheckNetworkMutex;
      bool forceCheckNetwork;  // true to force check of network interfaces

      TargetMap originalTargetMap;
      MirrorBuddyGroupMap originalMirrorBuddyGroupMap;
      MirrorBuddyGroupMap originalMetaMirrorBuddyGroupMap;

      virtual void run();
      void syncLoop();
      void handleNodeChanges(NodeType nodeType, NumNodeIDList& addedNodes,
         NumNodeIDList& removedNodes);
      void handleTargetMappingChanges();
      void handleBuddyGroupChanges();

      bool getAndResetForceCheckNetwork()
      {
         const std::lock_guard<Mutex> lock(forceCheckNetworkMutex);

         bool retVal = this->forceCheckNetwork;

         this->forceCheckNetwork = false;

         return retVal;
      }

      bool checkNetwork();

      Condition serversDownloadedCondition;
      Mutex serversDownloadedMutex;
      bool serversDownloaded;

   public:
      void waitForServers();

      void setForceCheckNetwork()
      {
         const std::lock_guard<Mutex> lock(forceCheckNetworkMutex);

         this->forceCheckNetwork = true;
      }

   private:
      static void printSyncNodesResults(NodeType nodeType, NumNodeIDList* addedNodes,
         NumNodeIDList* removedNodes);

      void setForceNodesAndTargetStatesUpdate()
      {
         const std::lock_guard<Mutex> lock(forceNodesAndTargetStatesUpdateMutex);

         this->forceNodesAndTargetStatesUpdate = true;
      }

      bool getAndResetForceNodesAndTargetStatesUpdate()
      {
         const std::lock_guard<Mutex> lock(forceNodesAndTargetStatesUpdateMutex);

         bool retVal = this->forceNodesAndTargetStatesUpdate;

         this->forceNodesAndTargetStatesUpdate = false;

         return retVal;
      }
};


#endif /* INTERNODESYNCER_H_ */
