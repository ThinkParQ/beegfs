#include <common/toolkit/MapTk.h>
#include "NumericIDMapper.h"

#include <climits>


#define NUMIDMAPPER_MAX_NUMIDS (USHRT_MAX-1) /* maximum number of IDs, -1 for reserved value "0" */
#define NUMIDMAPPER_MAX_NUMID  (USHRT_MAX)   /* highest valid ID value */


/**
 * Maps a string ID to a numeric ID and automatically generates a new numeric ID if no such ID was
 * assigned yet.
 *
 * @param stringID the stringID that should be mapped to a numericID.
 * @param currentNumID the current numeric ID that should be mapped, may be 0 if no ID was assigned
 * yet.
 * @param outNewNumID will be set to the new ID, if currentNumID was 0 (or to the given currentNumID
 * if it was accepted).
 * @return false if the given currentNumID conflicts with an already existing numID.
 */
bool NumericIDMapper::mapStringID(std::string stringID, uint16_t currentNumID,
   uint16_t* outNewNumID)
{
   RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

   bool retVal = false;

   NumericIDMapIter iter;

   if(currentNumID)
   { // numID given => check whether this ID is ok
      bool gotConflict = checkIDPairConflict(stringID, currentNumID);
      if(gotConflict)
         goto unlock_and_exit; // currentNumID conflicts with existing numeric ID

      idMap[stringID] = currentNumID;
      *outNewNumID = currentNumID;

      retVal = true;
      goto unlock_and_exit;
   }

   iter = idMap.find(stringID);
   if(iter == idMap.end() )
   { // didn't exist yet => generate a new ID (if we have free IDs)
      *outNewNumID = generateNumID(stringID);

      if(unlikely(!*outNewNumID) )
      {
         LogContext(__func__).logErr("Ran out of numeric IDs");
         goto unlock_and_exit;
      }

      // we found a free numID
      idMap[stringID] = *outNewNumID;
      retVal = true;
   }
   else
   { // stringID is mapped already => return numeric ID
      *outNewNumID = iter->second;
      retVal = true;
   }

unlock_and_exit:

   return retVal;
}

/**
 * @return true if stringID was found/mapped
 */
bool NumericIDMapper::unmapStringID(std::string stringID)
{
   RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

   size_t numErased = idMap.erase(stringID);

   return (numErased != 0);
}

/**
 * Generate a new numeric ID.
 * This method will also first check whether the given stringID already has a numeric ID assigned
 * and just didn't know of it yet (e.g. because of thread races during registration retries.)
 *
 * Note: Caller must hold read lock.
 * Note: Caller is expected to add map the ID pair (because new IDs are picked based on assigned IDs
 * in the map).
 *
 * @return 0 if all available numIDs are currently assigned, so none are left
 */
uint16_t NumericIDMapper::generateNumID(std::string stringID)
{
   // check whether this stringID is already associated with a numID

   uint16_t previousNumID = retrieveNumIDFromStringID(stringID);
   if(previousNumID)
      return previousNumID;

   // check if we have a numeric ID left

   size_t numIDsTotal = idMap.size();

   if(unlikely(numIDsTotal >= NUMIDMAPPER_MAX_NUMIDS) )
      return 0; // all numeric IDs currently in use


   /* walk from 1 to find the first free ID.
      (this could take quite some time if almost all numeric IDs are in use, but we assume that case
      is rare and we don't care too much about performance here.) */

   for(uint16_t newNumID=1; newNumID /* 0 means overflow */; newNumID++)
   {
      bool gotConflict = checkIDPairConflict(stringID, newNumID);
      if(!gotConflict)
         return newNumID; // we found an unused ID
   }

   return 0;
}

/**
 * Search for the given string ID and return it's associated numeric ID.
 *
 * Note: Unlocked, caller must hold read lock.
 *
 * @return 0 if the given stringID was not found, associated numeric ID otherwise.
 */
uint16_t NumericIDMapper::retrieveNumIDFromStringID(std::string stringID)
{
   NumericIDMapIter iter = idMap.find(stringID);
   if(iter != idMap.end() )
      return iter->second;

   return 0;
}

/**
 * Check whether we can grant a given pair of string and numeric ID.
 *
 * Note: Unlocked, caller must hold read-lock.
 *
 * @return true if the numeric ID is already assigned to another string ID or if this string ID
 * already has a different numeric ID assigned
 */
bool NumericIDMapper::checkIDPairConflict(std::string stringID, uint16_t numID)
{
   NumericIDMapIter iter = idMap.find(stringID);
   if(iter != idMap.end() )
   { // string ID already exists => check whether numID matches
      return (iter->second != numID);
   }

   // string ID doesn't exist yet => check whether given numID is used already

   for(iter = idMap.begin(); iter != idMap.end(); iter++)
   {
      if(iter->second == numID)
         return true;
   }

   // no conflicts found

   return false;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool NumericIDMapper::loadFromFile()
{
   const char* logContext = "NumericIDMapper (load)";

   bool loaded = false;

   RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

   if(!storePath.length() )
      goto unlock_and_exit;

   try
   {
      StringMap newIDStrMap;

      // load from file
      MapTk::loadStringMapFromFile(storePath.c_str(), &newIDStrMap);

      // apply loaded ID mappings
      idMap.clear();
      importFromStringMap(newIDStrMap);

      loaded = true;
   }
   catch(InvalidConfigException& e)
   {
      LOG_DEBUG(logContext, Log_DEBUG, "Unable to open ID mappings file: " + storePath + ". "
         "SysErr: " + System::getErrString() );
      IGNORE_UNUSED_VARIABLE(logContext);
   }

unlock_and_exit:
   return loaded;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool NumericIDMapper::saveToFile()
{
   const char* logContext = "NumericIDMapper (save)";

   bool saved = false;

   RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

   if(storePath.empty() )
      goto unlock_and_exit;

   try
   {
      StringMap idStrMap;

      exportToStringMap(idStrMap);

      MapTk::saveStringMapToFile(storePath.c_str(), &idStrMap);

      saved = true;
   }
   catch(InvalidConfigException& e)
   {
      LogContext(logContext).logErr("Unable to save target mappings file: " + storePath + ". "
         "SysErr: " + System::getErrString() );
   }

unlock_and_exit:
   return saved;
}

/**
 * Note: unlocked, caller must hold lock.
 */
void NumericIDMapper::exportToStringMap(StringMap& outExportMap)
{
   for(NumericIDMapIter iter = idMap.begin(); iter != idMap.end(); iter++)
      outExportMap[iter->first] = StringTk::uintToHexStr(iter->second);
}

/**
 * Note: unlocked, caller must hold lock.
 * Note: the internal idMap is not cleared in this method.
 */
void NumericIDMapper::importFromStringMap(StringMap& importMap)
{
   for(StringMapIter iter = importMap.begin(); iter != importMap.end(); iter++)
      idMap[iter->first] = StringTk::strHexToUInt(iter->second);
}

