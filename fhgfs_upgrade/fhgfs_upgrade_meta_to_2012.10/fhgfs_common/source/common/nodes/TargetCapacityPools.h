#ifndef TARGETCAPACITYPOOLS_H_
#define TARGETCAPACITYPOOLS_H_

#include <common/Common.h>
#include <common/threading/SafeRWLock.h>
#include <common/toolkit/RandomReentrant.h>


enum TargetCapacityPoolType
   {TargetCapacityPool_NORMAL=0, TargetCapacityPool_LOW=1, TargetCapacityPool_EMERGENCY=2};


/**
 * Note: We also use these pools to store nodeIDs (instead of targetIDs) for meta servers.
 */
class TargetCapacityPools
{
   public:
      TargetCapacityPools(int64_t lowSpace, int64_t emergencySpace);

      bool addOrUpdate(uint16_t targetID, TargetCapacityPoolType poolType);
      bool addIfNotExists(uint16_t targetID, TargetCapacityPoolType poolType);
      void remove(uint16_t targetID);

      void syncPoolsFromLists(UInt16List& listNormal, UInt16List& listLow,
         UInt16List& listEmergency);
      void getPoolsAsLists(UInt16List& outListNormal, UInt16List& outListLow,
         UInt16List& outListEmergency);

      void chooseStorageTargets(unsigned numTargets, UInt16List* preferredTargets,
         UInt16Vector* outTargets);
      void chooseStorageTargetsRoundRobin(unsigned numTargets, UInt16Vector* outTargets);


      static const char* poolTypeToStr(TargetCapacityPoolType poolType);


   private:
      RWLock rwlock;

      int64_t lowSpace;
      int64_t emergencySpace;

      UInt16Set poolNormal; // (keys: targetIDs)
      UInt16Set poolLow; // getting low on free space (keys: targetIDs)
      UInt16Set poolEmergency; // extremely low on free space (keys: targetIDs)

      RandomReentrant randGen;

      uint16_t lastRoundRobinTarget; // used for round-robin chooser

      void addUncheckedUnlocked(uint16_t targetID, TargetCapacityPoolType poolType);
      void removeFromOthersUnlocked(uint16_t targetID, TargetCapacityPoolType keepPool);

      UInt16Set* getPoolFromTypeUnlocked(TargetCapacityPoolType poolType);

      void chooseStorageNodesNoPref(UInt16Set& activeTargets, unsigned numTargets,
         UInt16Vector* outTargets);
      void chooseStorageNodesNoPrefRoundRobin(UInt16Set& activeTargets, unsigned numTargets,
         UInt16Vector* outTargets);
      void chooseStorageNodesWithPref(UInt16Set& activeTargets, unsigned numTargets,
         UInt16List* preferredTargets, bool allowNonPreferredTargets, UInt16Vector* outTargets);


   public:
      // getters & setters
      TargetCapacityPoolType getPoolTypeFromFreeSpace(int64_t freeSpace)
      {
         if(freeSpace > lowSpace)
            return TargetCapacityPool_NORMAL;

         if(freeSpace > emergencySpace)
            return TargetCapacityPool_LOW;

         return TargetCapacityPool_EMERGENCY;
      }


   private:
      // template inliners

      /**
       * Note: static method => unlocked (=> caller must hold read lock)
       */
      template <class TCollection, class TIterator>
      static void moveIterToNextRingElem(TCollection& collection,
         TIterator& iter)
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
      void moveIterToRandomElem(TCollection& collection,
         TIterator& iter)
      {
         iter = collection.begin();

         if(collection.size() < 2)
            return;

         int randInt = randGen.getNextInRange(0, collection.size()-1);

         while(randInt--)
            iter++;
      }


};

#endif /* TARGETCAPACITYPOOLS_H_ */
