#include "TargetMapperEx.h"

#include <common/toolkit/MapTk.h>

/**
 * Note: setStorePath must be called before using this.
 *
 * @return true if file was successfully saved
 */
bool TargetMapperEx::saveToFile()
{
   const char* logContext = "TargetMapper (save)";

   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE); // L O C K

   if(storePath.empty() )
      return false;

   try
   {
      StringMap targetsStrMap;

      exportToStringMap(targetsStrMap);

      MapTk::saveStringMapToFile(storePath.c_str(), &targetsStrMap);

      return true;
   }
   catch(InvalidConfigException& e)
   {
      LogContext(logContext).logErr("Unable to save target mappings file: " + storePath + ". " +
         "SysErr: " + System::getErrString() );
      IGNORE_UNUSED_VARIABLE(logContext);

      return false;
   }
}

/**
 * Note: setStorePath must be called before using this.
 */
bool TargetMapperEx::loadFromFile()
{
   const char* logContext = "TargetMapper (load)";

   bool loaded = false;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   if(!storePath.length() )
      goto unlock_and_exit;

   try
   {
      StringMap newTargetsStrMap;

      // load from file
      MapTk::loadStringMapFromFile(storePath.c_str(), &newTargetsStrMap);

      // apply loaded targets
      targets.clear();
      importFromStringMap(newTargetsStrMap);

      for (TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
      {
         // add to attached states
         if (states)
         {
            const CombinedTargetState defaultTargetState(TargetReachabilityState_POFFLINE,
                  TargetConsistencyState_GOOD);
            CombinedTargetState ignoredReturnState;

            if (!states->getState(iter->first, ignoredReturnState)) {
               LogContext(logContext).log(Log_WARNING,
                     "Storage target " + StringTk::intToStr(iter->first)
                     + " missing in targetStates. Consistency state is lost.");
               LogContext(logContext).log(Log_WARNING, "Adding default state for storage target "
                     + StringTk::intToStr(iter->first) + ": "
                     + TargetStateStore::stateToStr(defaultTargetState));
               IGNORE_UNUSED_VARIABLE(logContext);

               states->addIfNotExists(iter->first, defaultTargetState);
            }
         }

         // add to attached storage pools
         // NOTE: only targets, which are not already present in a pool are added. This should usually
         // not happen, but is normal after an update from BeeGFS v6
         if (storagePools)
         {
            storagePools->addTarget(iter->first, iter->second,  StoragePoolStore::DEFAULT_POOL_ID,
                                    true);
         }

         // add to attached exceededQuotaStores
         if (exceededQuotaStores)
         {
            exceededQuotaStores->add(iter->first);
         }
      }

      loaded = true;
   }
   catch(InvalidConfigException& e)
   {
      LOG_DEBUG(logContext, Log_DEBUG, "Unable to open target mappings file: " + storePath + ". " +
         "SysErr: " + System::getErrString() );
      IGNORE_UNUSED_VARIABLE(logContext);
   }

unlock_and_exit:

   safeLock.unlock(); // U N L O C K

   return loaded;
}

/**
 * Note: unlocked, caller must hold lock.
 */
void TargetMapperEx::exportToStringMap(StringMap& outExportMap) const
{
   for(TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
   {
      outExportMap[StringTk::uintToHexStr(iter->first) ] = iter->second.strHex();
   }
}

/**
 * Note: unlocked, caller must hold lock.
 * Note: the internal targets map is not cleared in this method.
 */
void TargetMapperEx::importFromStringMap(StringMap& importMap)
{
   for(StringMapIter iter = importMap.begin(); iter != importMap.end(); iter++)
   {
      targets[StringTk::strHexToUInt(iter->first) ] =
         NumNodeID(StringTk::strHexToUInt(iter->second) );
   }
}
