#ifndef TARGETCAPACITYPOOLS_H_
#define TARGETCAPACITYPOOLS_H_

#include <common/Common.h>
#include <common/nodes/DynamicPoolLimits.h>
#include <common/nodes/CapacityPoolType.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/StoragePoolId.h>
#include <common/threading/RWLock.h>
#include <common/toolkit/RandomReentrant.h>


typedef std::map<uint16_t, NumNodeID> TargetMap; // keys: targetIDs, values: nodeNumIDs
typedef TargetMap::iterator TargetMapIter;
typedef TargetMap::const_iterator TargetMapCIter;
typedef TargetMap::value_type TargetMapVal;

typedef std::map<NumNodeID, UInt16Set> GroupedTargets; // keys: nodeIDs, values: node's targetIDs
typedef GroupedTargets::iterator GroupedTargetsIter;
typedef GroupedTargets::const_iterator GroupedTargetsCIter;
typedef GroupedTargets::value_type GroupedTargetsVal;

typedef std::vector<GroupedTargets> GroupedTargetsVector; // CapacityPoolType is index
typedef GroupedTargetsVector::iterator GroupedTargetsVectorIter;
typedef GroupedTargetsVector::const_iterator GroupedTargetsVectorConstIter;

/**
 * This class provides pools of targetIDs based on their free space.
 * There are two internal types of pools: The general pools with all targets of the corresponding
 * free space class; and a pools that are grouped by nodeIDs to provide the ability to select
 * targets from different nodes (different failure domains).
 *
 * Note: As mappings from targetIDs to nodes can be unknown for some targets, it is possible that
 * not all targets are contained in the per-node groups.
 */
class TargetCapacityPools
{
   public:
      TargetCapacityPools(bool useDynamicPools, const DynamicPoolLimits& poolLimitsSpace,
            const DynamicPoolLimits& poolLimitsInodes);

      bool addOrUpdate(uint16_t targetID, NumNodeID nodeID, CapacityPoolType poolType);
      bool addIfNotExists(uint16_t targetID, NumNodeID nodeID, CapacityPoolType poolType);
      void remove(uint16_t targetID);

      void syncPoolsFromLists(const UInt16ListVector& lists, TargetMap& newTargetMap);
      UInt16ListVector getPoolsAsLists() const;

      void chooseStorageTargets(unsigned numTargets, unsigned minNumRequiredTargets,
         const UInt16List* preferredTargets, UInt16Vector* outTargets);
      void chooseStorageTargetsRoundRobin(unsigned numTargets, UInt16Vector* outTargets);
      void chooseTargetsInterdomain(unsigned numTargets, unsigned minNumRequiredTargets,
         UInt16Vector* outTargets);
      void chooseTargetsIntradomain(unsigned numTargets, unsigned minNumRequiredTargets,
         UInt16Vector* outTargets);

      bool getPoolAssignment(uint16_t targetID, CapacityPoolType* outPoolType) const;

      std::string getStateAsStr() const;

      static const char* poolTypeToStr(CapacityPoolType poolType);

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
          ctx
             % obj->pools
             % obj->groupedTargetPools
             % obj->targetMap;
      }

   private:
      mutable RWLock rwlock;

      bool dynamicPoolsEnabled;
      const DynamicPoolLimits poolLimitsSpace;
      const DynamicPoolLimits poolLimitsInodes;

      UInt16SetVector pools;

      GroupedTargetsVector groupedTargetPools; // targets grouped by node
      TargetMap targetMap; // the basis of our groupedTargets assignments

      RandomReentrant randGen; // for random target selection
      uint16_t lastRoundRobinTarget; // used for round-robin chooser

      bool addOrUpdateUnlocked(uint16_t targetID, NumNodeID nodeID, CapacityPoolType poolType);
      void removeFromOthersUnlocked(uint16_t targetID, NumNodeID previousNodeID,
         CapacityPoolType keepPool);
      void chooseStorageNodesNoPref(const UInt16Set& activeTargets, unsigned numTargets,
         UInt16Vector* outTargets);
      void chooseStorageNodesNoPrefRoundRobin(const UInt16Set& activeTargets, unsigned numTargets,
         UInt16Vector* outTargets);
      void chooseTargetsInterdomainNoPref(const GroupedTargets& groupedTargets, unsigned numTargets,
         UInt16Vector& outTargets, NumNodeIDVector& outNodes);
      void chooseTargetsIntradomainNoPref(const GroupedTargets& groupedTargets, unsigned numTargets,
         UInt16Vector& outTargets, UInt16Vector& outGroups);
      void chooseStorageNodesWithPref(const UInt16Set& activeTargets, unsigned numTargets,
         const UInt16List* preferredTargets, bool allowNonPreferredTargets,
         UInt16Vector* outTargets, std::set<uint16_t>& chosenTargets);

      static void groupTargetsByNode(const UInt16SetVector& pools, const TargetMap& targetMap,
         GroupedTargetsVector* outGroupedTargetPools);
      static void copyAndStripGroupedTargets(const GroupedTargets& groupedTargets,
         const NumNodeIDVector& stripNodes, GroupedTargets& outGroupedTargetsStripped);

      static NumNodeID getNodeIDFromTargetID(uint16_t targetID, const TargetMap& targetMap);


   public:
      // getters & setters

      bool getDynamicPoolsEnabled() const
      {
         return this->dynamicPoolsEnabled;
      }

      const DynamicPoolLimits& getPoolLimitsSpace() const
      {
         return poolLimitsSpace;
      }

      const DynamicPoolLimits& getPoolLimitsInodes() const
      {
         return poolLimitsInodes;
      }

   private:
      // template inliners

      /**
       * Note: static method => unlocked (=> caller must hold read lock)
       */
      template <class TCollection, class TIterator>
      static void moveIterToNextRingElem(TCollection& collection, TIterator& iter)
      {
         if(iter != collection.end() )
         {
            iter++;
            if(iter == collection.end() )
               iter = collection.begin();
         }
         else
         { // iterator is pointing to the end element
            iter = collection.begin(); // this should actually never happen
         }
      }

      /**
       * Note: unlocked (=> caller must hold read lock)
       */
      template <class TCollection, class TIterator>
      void moveIterToRandomElem(TCollection& collection, TIterator& iter)
      {
         iter = collection.begin();

         int collectionSize = collection.size();

         if(collectionSize < 2)
            return;

         int randInt = randGen.getNextInRange(0, collectionSize - 1);

         while(randInt--)
            iter++;
      }


};

#endif /* TARGETCAPACITYPOOLS_H_ */
