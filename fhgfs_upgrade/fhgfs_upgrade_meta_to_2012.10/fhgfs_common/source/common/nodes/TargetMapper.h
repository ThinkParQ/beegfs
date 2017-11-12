#ifndef TARGETMAPPER_H_
#define TARGETMAPPER_H_

#include <common/nodes/TargetCapacityPools.h>
#include <common/threading/SafeRWLock.h>
#include <common/threading/SafeMutexLock.h>
#include <common/Common.h>


class TargetMapper
{
   // internal type definitions...

   typedef std::map<uint16_t, uint16_t> TargetMap; // keys: targetIDs, values: nodeNumIDs
   typedef TargetMap::iterator TargetMapIter;
   typedef TargetMap::const_iterator TargetMapCIter;
   typedef TargetMap::value_type TargetMapVal;


   public:
      TargetMapper();

      bool mapTarget(uint16_t targetID, uint16_t nodeID);
      bool unmapTarget(uint16_t targetID);
      bool unmapByNodeID(uint16_t nodeID);

      void syncTargetsFromLists(UInt16List& targetIDs, UInt16List& nodeIDs);
      void getMappingAsLists(UInt16List& outTargetIDs, UInt16List& outNodeIDs);
      void getTargetsByNode(uint16_t nodeID, UInt16List& outTargetIDs);

      void attachCapacityPools(TargetCapacityPools* capacityPools);

      bool loadFromFile();
      bool saveToFile();


   private:
      RWLock rwlock;
      TargetMap targets; // keys: targetIDs, values: nodeNumIDs

      TargetCapacityPools* capacityPools; // optional for auto add/remove on map/unmap (may be NULL)

      bool mappingsDirty; // true if saved mappings file needs to be updated
      std::string storePath; // set to enable load/save methods (setting is not thread-safe)

      void exportToStringMap(StringMap& outExportMap);
      void importFromStringMap(StringMap& importMap);


   public:
      // getters & setters

      /**
       * @return 0 if target not found
       */
      uint16_t getNodeID(uint16_t targetID)
      {
         uint16_t nodeID = 0;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         TargetMapCIter iter = targets.find(targetID);
         if(iter != targets.end() )
         { // not in map yet => try to load it
            nodeID = iter->second;
         }

         safeLock.unlock(); // U N L O C K

         return nodeID;
      }

      size_t getSize()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         size_t targetsSize = targets.size();

         safeLock.unlock(); // U N L O C K

         return targetsSize;
      }

      void setStorePath(std::string storePath)
      {
         this->storePath = storePath;
      }

      bool isMapperDirty()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         bool retVal = mappingsDirty;

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

};

#endif /* TARGETMAPPER_H_ */
