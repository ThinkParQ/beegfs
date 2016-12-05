#ifndef MGMTDTARGETSTATESTORE_H_
#define MGMTDTARGETSTATESTORE_H_

#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/TargetStateStore.h>
#include <common/nodes/Node.h>

class MgmtdTargetStateStore : public TargetStateStore
{
   public:
      MgmtdTargetStateStore(NodeType nodeType);
      void setConsistencyStatesFromLists(const UInt16List& targetIDs,
         const UInt8List& consistencyStates, bool setOnline);
      FhgfsOpsErr changeConsistencyStatesFromLists(const UInt16List& targetIDs,
         const UInt8List& oldStates, const UInt8List& newStates,
         MirrorBuddyGroupMapper* buddyGroups);

      bool autoOfflineTargets(const unsigned pofflineTimeout, const unsigned offlineTimeout,
         MirrorBuddyGroupMapper* buddyGroups);
      bool resolveDoubleResync();
      bool resolvePrimaryResync();

      void saveResyncSetToFile();
      bool loadResyncSetFromFile() throw (InvalidConfigException);

   private:
      typedef std::set<uint16_t> TargetIDSet;
      typedef TargetIDSet::iterator TargetIDSetIter;
      typedef TargetIDSet::const_iterator TargetIDSetCIter;

      // For persistent storage of storage target / metadata node ids that need resync.
      RWLock resyncSetLock;
      TargetIDSet resyncSet;
      bool resyncSetDirty;
      std::string resyncSetStorePath;

      NodeType nodeType; // Meta node state store or storage target state store.

      UInt16Vector findDoubleResync();
      UInt16Vector findPrimaryResync();
      bool setTargetsGood(const UInt16Vector& ids);

   public:
      // getters & setters

      bool isResyncSetDirty()
      {
         SafeRWLock safeLock(&resyncSetLock, SafeRWLock_READ); // L O C K

         bool res = resyncSetDirty;

         safeLock.unlock(); // U N L O C K

         return res;
      }

      void setResyncSetStorePath(const std::string& storePath)
      {
         this->resyncSetStorePath = storePath;
      }


   private:

      void resyncSetUpdate(TargetConsistencyState consistencyState, uint16_t targetID)
      {
         SafeRWLock safeLock(&resyncSetLock, SafeRWLock_WRITE); // L O C K

         bool changed = false;

         if (consistencyState == TargetConsistencyState_NEEDS_RESYNC)
            changed = resyncSet.insert(targetID).second;
         else
            changed = (resyncSet.erase(targetID) > 0);

         resyncSetDirty |= changed;

         safeLock.unlock(); // L O C K
      }

      /**
       * Sets the consistency state of a single target, not changing the reachability state.
       * @returns false if target ID was not found, true otherwise.
       */
      bool setConsistencyState(const uint16_t targetID, const TargetConsistencyState state)
      {
         bool res;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         TargetStateInfoMapIter iter = this->statesMap.find(targetID);

         if (iter == this->statesMap.end() )
            res = false;
         else
         {
            iter->second.consistencyState = state;
            res = true;
         }

         safeLock.unlock(); // U N L O C K

         return res;
      }

      /**
       * @returns an std::string that contains either "Storage target" or "Metadata node" depending
       *          on the nodeType of this state store.
       */
      std::string nodeTypeStr(bool uppercase)
      {
         std::string res;
         switch (this->nodeType)
         {
            case NODETYPE_Storage:
               res = "storage target";
               break;
            case NODETYPE_Meta:
               res = "metadata node";
               break;
            default:
               res = "node";
               break;
         }

         if (uppercase)
            res[0] = ::toupper(res[0]);

         return res;
      }
};

#endif /*MGMTDTARGETSTATESTORE_H_*/
