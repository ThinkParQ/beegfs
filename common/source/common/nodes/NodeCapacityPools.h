#ifndef NODECAPACITYPOOLS_H_
#define NODECAPACITYPOOLS_H_

#include <common/Common.h>
#include <common/nodes/TargetCapacityPools.h>
#include <common/nodes/DynamicPoolLimits.h>
#include <common/toolkit/RandomReentrant.h>


/**
 * Note: We also use these pools to store nodeIDs (instead of targetIDs) for meta servers.
 */
class NodeCapacityPools
{
   public:
      NodeCapacityPools(bool useDynamicPools, const DynamicPoolLimits& poolLimitsSpace,
            const DynamicPoolLimits& poolLimitsInodes);

      bool addOrUpdate(uint16_t targetID, CapacityPoolType poolType);
      bool addIfNotExists(uint16_t targetID, CapacityPoolType poolType);
      void remove(uint16_t targetID);

      void syncPoolsFromLists(const UInt16ListVector& lists);
      void syncPoolsFromSets(UInt16SetVector& sets);
      UInt16ListVector getPoolsAsLists() const;

      void chooseStorageTargets(unsigned numTargets, unsigned minNumRequiredTargets,
         const UInt16List* preferredTargets, UInt16Vector* outTargets);
      void chooseStorageTargetsRoundRobin(unsigned numTargets, UInt16Vector* outTargets);

      bool getPoolAssignment(uint16_t nodeID, CapacityPoolType* outPoolType) const;

      std::string getStateAsStr() const;

      static const char* poolTypeToStr(CapacityPoolType poolType);

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
          ctx
             % obj->pools;
      }

   private:
      mutable RWLock rwlock;

      bool useDynamicPools;
      const DynamicPoolLimits poolLimitsSpace;
      const DynamicPoolLimits poolLimitsInodes;

      UInt16SetVector pools;

      RandomReentrant randGen; // for random target selection
      uint16_t lastRoundRobinTarget; // used for round-robin chooser

      void removeFromOthersUnlocked(uint16_t targetID, CapacityPoolType keepPool);

      void chooseStorageNodesNoPref(UInt16Set& activeTargets, unsigned numTargets,
         UInt16Vector* outTargets);
      void chooseStorageNodesNoPrefRoundRobin(UInt16Set& activeTargets, unsigned numTargets,
         UInt16Vector* outTargets);
      void chooseStorageNodesWithPref(UInt16Set& activeTargets, unsigned numTargets,
         const UInt16List* preferredTargets, bool allowNonPreferredTargets,
         UInt16Vector* outTargets, std::set<uint16_t>& chosenTargets);

   public:
      // getters & setters
      bool getDynamicPoolsEnabled() const
      {
         return this->useDynamicPools;
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
      template <class TCollection>
      static void moveIterToNextRingElem(const TCollection& collection,
         typename TCollection::const_iterator& iter)
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
      template <class TCollection>
      void moveIterToRandomElem(const TCollection& collection,
         typename TCollection::const_iterator& iter)
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


#endif /* NODECAPACITYPOOLS_H_ */
