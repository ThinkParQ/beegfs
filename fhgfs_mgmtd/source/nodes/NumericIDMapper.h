#ifndef NUMERICIDMAPPER_H_
#define NUMERICIDMAPPER_H_

#include <common/threading/SafeRWLock.h>
#include <common/toolkit/Random.h>
#include <common/Common.h>


/**
 * Maps long string IDs to short numeric IDs.
 *
 * Currently only used for targets
 *
 * The numeric value "0" is reserved for invalid IDs.
 */
class NumericIDMapper
{
   public:
      // type definitions

      typedef std::map<std::string, uint16_t> NumericIDMap;
      typedef NumericIDMap::iterator NumericIDMapIter;
      typedef NumericIDMap::const_iterator NumericIDMapCIter;
      typedef NumericIDMap::value_type NumericIDMapVal;


      NumericIDMapper();

      bool mapStringID(std::string stringID, uint16_t currentNumID, uint16_t* outNewNumID);
      bool unmapStringID(std::string stringID);

      bool loadFromFile();
      bool saveToFile();


   private:
      RWLock rwlock;
      NumericIDMap idMap; // keys: string IDs, values: numeric IDs

      Random randGen; // for random new IDs; must also be synchronized by lock

      bool mappingsDirty; // true if saved mappings file needs to be updated
      std::string storePath; // set to enable load/save methods (setting is not thread-safe)


      uint16_t generateNumID(std::string stringID);
      uint16_t retrieveNumIDFromStringID(std::string stringID);
      bool checkIDPairConflict(std::string stringID, uint16_t numID);

      void exportToStringMap(StringMap& outExportMap);
      void importFromStringMap(StringMap& importMap);


   public:
      // getters & setters

      /**
       * Get mapped numeric ID for a given string ID.
       *
       * @return 0 if stringID not found
       */
      uint16_t getNumericID(std::string stringID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         uint16_t numericID = 0;

         NumericIDMapIter iter = idMap.find(stringID);
         if(iter != idMap.end() )
         { // not in map yet => try to load it
            numericID = iter->second;
         }

         safeLock.unlock(); // U N L O C K

         return numericID;
      }

      size_t getSize()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         size_t mapSize = idMap.size();

         safeLock.unlock(); // U N L O C K

         return mapSize;
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


#endif /* NUMERICIDMAPPER_H_ */
