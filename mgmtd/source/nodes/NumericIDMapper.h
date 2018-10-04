#ifndef NUMERICIDMAPPER_H_
#define NUMERICIDMAPPER_H_

#include <common/Common.h>
#include <common/threading/RWLockGuard.h>


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


      bool mapStringID(std::string stringID, uint16_t currentNumID, uint16_t* outNewNumID);
      bool unmapStringID(std::string stringID);

      bool loadFromFile();
      bool saveToFile();


   private:
      mutable RWLock rwlock;
      NumericIDMap idMap; // keys: string IDs, values: numeric IDs

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
      uint16_t getNumericID(std::string stringID) const
      {
         RWLockGuard const lock(rwlock, SafeRWLock_READ);

         NumericIDMapCIter iter = idMap.find(stringID);
         if(iter != idMap.end() )
            return iter->second;

         return 0;
      }

      size_t getSize() const
      {
         RWLockGuard const lock(rwlock, SafeRWLock_READ);

         return idMap.size();
      }

      void setStorePath(std::string storePath)
      {
         this->storePath = storePath;
      }
};


#endif /* NUMERICIDMAPPER_H_ */
