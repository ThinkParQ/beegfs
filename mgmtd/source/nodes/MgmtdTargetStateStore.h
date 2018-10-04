#ifndef MGMTDTARGETSTATESTORE_H_
#define MGMTDTARGETSTATESTORE_H_

#include <nodes/MirrorBuddyGroupMapperEx.h>
#include <common/nodes/TargetStateStore.h>

class MgmtdTargetStateStore : public TargetStateStore
{
   public:
      MgmtdTargetStateStore(NodeType nodeType);
      void setConsistencyStatesFromLists(const UInt16List& targetIDs,
         const UInt8List& consistencyStates, bool setOnline);
      FhgfsOpsErr changeConsistencyStatesFromLists(const UInt16List& targetIDs,
         const UInt8List& oldStates, const UInt8List& newStates,
         MirrorBuddyGroupMapperEx* buddyGroups);

      bool autoOfflineTargets(const unsigned pofflineTimeoutMS, const unsigned offlineTimeoutMS,
         MirrorBuddyGroupMapperEx* buddyGroups);
      bool resolvePrimaryResync();

      bool loadResyncSetFromFile();
      void saveStatesToFile();
      bool loadStatesFromFile();

   private:
      typedef std::set<uint16_t> TargetIDSet;
      typedef TargetIDSet::iterator TargetIDSetIter;
      typedef TargetIDSet::const_iterator TargetIDSetCIter;

      // For persistent storage of storage target / metadata node ids that need resync.
      TargetIDSet tempResyncSet;
      std::string resyncSetStorePath;
      std::string targetStateStorePath;

      UInt16Vector findPrimaryResync();
      bool addOrUpdate(uint16_t targetID, CombinedTargetState state);

   public:
      // getters & setters

      void setResyncSetStorePath(const std::string& storePath)
      {
         this->resyncSetStorePath = storePath;
      }

      const std::string& getResyncSetStorePath() const
      {
         return resyncSetStorePath;
      }

      void setTargetStateStorePath(const std::string& storePath)
      {
         targetStateStorePath = storePath;
      }

      NodeType getNodeType() const { return nodeType; }

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

   private:

      /**
       * Sets the consistency state of a single target, not changing the reachability state.
       * @returns false if target ID was not found, true otherwise.
       */
      bool setConsistencyState(const uint16_t targetID, const TargetConsistencyState state)
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);

         TargetStateInfoMapIter iter = this->statesMap.find(targetID);

         if (iter == this->statesMap.end() )
            return false;

         iter->second.consistencyState = state;
         return true;
      }
};

#endif /*MGMTDTARGETSTATESTORE_H_*/
