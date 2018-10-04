#include "MirrorBuddyGroupMapperEx.h"

#include <common/toolkit/MapTk.h>

bool MirrorBuddyGroupMapperEx::loadFromFile()
{
   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);

   if ( !storePath.length() )
      return false;

   try
   {
      StringMap newGroupsStrMap;

      // load from file
      MapTk::loadStringMapFromFile(storePath.c_str(), &newGroupsStrMap);

      // apply loaded targets
      mirrorBuddyGroups.clear();
      importFromStringMap(newGroupsStrMap);

      return true;
   }
   catch (InvalidConfigException& e)
   {
      LOG_DEBUG(__func__, Log_DEBUG, "Unable to open mirror buddy groups mappings file: "
         + storePath + ". " + "SysErr: " + System::getErrString());

      return false;
   }
}

bool MirrorBuddyGroupMapperEx::saveToFile()
{
   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);

   if ( storePath.empty() )
      return false;

   try
   {
      StringMap groupsStrMap;

      exportToStringMap(groupsStrMap);

      MapTk::saveStringMapToFile(storePath.c_str(), &groupsStrMap);

      return true;
   }
   catch (InvalidConfigException& e)
   {
      LogContext(__func__).logErr(
         "Unable to save mirror buddy groups mappings file: " + storePath + ". " + "SysErr: "
            + System::getErrString());

      return false;
   }
}

/**
 * Note: unlocked, caller must hold lock.
 */
void MirrorBuddyGroupMapperEx::exportToStringMap(StringMap& outExportMap) const
{
   for ( MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      const MirrorBuddyGroup mbg = iter->second;

      const std::string mbgStr = StringTk::uint16ToHexStr(mbg.firstTargetID) + ","
         + StringTk::uint16ToHexStr(mbg.secondTargetID);

      outExportMap[StringTk::uintToHexStr(iter->first)] = mbgStr;
   }
}

/**
 * Note: unlocked, caller must hold lock.
 * Note: the internal map is not cleared in this method.
 */
void MirrorBuddyGroupMapperEx::importFromStringMap(StringMap& importMap)
{
   for(StringMapIter iter = importMap.begin(); iter != importMap.end(); iter++)
   {
      const uint16_t groupID = StringTk::strHexToUInt(iter->first);
      const std::string mbgStr = iter->second;
      StringVector mbgStrVec;

      StringTk::explode(mbgStr, ',', &mbgStrVec);

      if (mbgStrVec.size() > 1)
      {
         const uint16_t primaryTargetID = StringTk::strHexToUInt(mbgStrVec[0]);
         const uint16_t secondaryTargetID = StringTk::strHexToUInt(mbgStrVec[1]);
         mirrorBuddyGroups[groupID] = MirrorBuddyGroup(primaryTargetID, secondaryTargetID);

         // add to attached storage pools
         // NOTE: only groups, which are not already present in a pool are added. This should
         // usually not happen, but is normal after an update from BeeGFS v6
         if (storagePools)
         {
            // automatically add the buddy group to the pool of the primary target
            const StoragePoolId poolID = storagePools->getPool(primaryTargetID)->getId();
            storagePools->addBuddyGroup(groupID, poolID, true);
         }
      }
      else
      {
         LogContext(__func__).logErr("Corrupt importMap supplied");
         mirrorBuddyGroups[groupID] = MirrorBuddyGroup(0, 0);
      }
   }
}

/**
 * Switch buddy buddy group's primary and secondary targets.
 *
 * Note: Unlocked, caller must hold writelock.
 * Note: Private, but intended to be called by friend class MgmtdTargetStateStore.
 */
void MirrorBuddyGroupMapperEx::switchover(uint16_t buddyGroupID)
{
   const char* logContext = "Primary/secondary switch";

   MirrorBuddyGroupMapIter iter = mirrorBuddyGroups.find(buddyGroupID);

   if (unlikely(iter == mirrorBuddyGroups.end() ) )
   {
      LogContext(logContext).log(Log_ERR, "Unknown mirror group ID: "
         + StringTk::uintToStr(buddyGroupID) );

      return;
   }

   MirrorBuddyGroup& mbg = iter->second;

   LogContext(logContext).log(Log_WARNING, "Switching primary and secondary target. "
         "Mirror group ID: "+ StringTk::uintToStr(iter->first) );

   std::swap(mbg.firstTargetID, mbg.secondTargetID);
}
