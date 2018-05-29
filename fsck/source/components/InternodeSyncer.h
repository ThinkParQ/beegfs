#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/threading/PThread.h>
#include <common/Common.h>


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

      TargetMap originalTargetMap;
      MirrorBuddyGroupMap originalMirrorBuddyGroupMap;
      MirrorBuddyGroupMap originalMetaMirrorBuddyGroupMap;

      virtual void run();
      void syncLoop();
      void handleNodeChanges(NodeType nodeType, NumNodeIDList& addedNodes,
         NumNodeIDList& removedNodes);
      void handleTargetMappingChanges();
      void handleBuddyGroupChanges();

      Condition serversDownloadedCondition;
      Mutex serversDownloadedMutex;
      bool serversDownloaded;

   public:
      void waitForServers();

   private:
      static void printSyncNodesResults(NodeType nodeType, NumNodeIDList* addedNodes,
         NumNodeIDList* removedNodes);

      void setForceNodesAndTargetStatesUpdate()
      {
         SafeMutexLock safeLock(&forceNodesAndTargetStatesUpdateMutex);

         this->forceNodesAndTargetStatesUpdate = true;

         safeLock.unlock();
      }

      bool getAndResetForceNodesAndTargetStatesUpdate()
      {
         SafeMutexLock safeLock(&forceNodesAndTargetStatesUpdateMutex);

         bool retVal = this->forceNodesAndTargetStatesUpdate;

         this->forceNodesAndTargetStatesUpdate = false;

         safeLock.unlock();

         return retVal;
      }
};


#endif /* INTERNODESYNCER_H_ */
