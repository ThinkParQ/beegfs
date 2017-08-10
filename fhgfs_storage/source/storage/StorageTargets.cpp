#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/storage/Storagedata.h>
#include <common/threading/RWLockGuard.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include "StorageTargets.h"


#define STORAGETARGETS_CONFIG_DELIMITER   ','

#define STORAGETARGETS_TARGETDIR_MKDIR_MODE (S_IRWXU)
#define STORAGETARGETS_CHUNKDIR_MKDIR_MODE (S_IRWXU | S_IRWXG | S_IRWXO)


/**
 * Note: Initialization is split into two steps. The first one is read-only loading followed by
 * locking and the second one is preparation of dirs, subdirs etc.
 * So make sure to call prepareTargetDirs() later.
 */
StorageTargets::StorageTargets(Config* cfg) throw(InvalidConfigException)
   : targetOfflineWait(cfg)
{
   // step one (of two): read-only init...

   initTargetPathsList(cfg);

   initQuotaBlockDevices();

   lockTargetPaths();
}

StorageTargets::~StorageTargets()
{
   removeAllTargetDirs();
   unlockTargetPaths();
}

/**
 * Fill internal target paths list from config and check validity.
 */
void StorageTargets::initTargetPathsList(Config* cfg) throw(InvalidConfigException)
{
   if(cfg->getStoreStorageDirectoryV2().empty() )
      throw InvalidConfigException("No storage target defined");

   StringTk::explode(cfg->getStoreStorageDirectoryV2(), STORAGETARGETS_CONFIG_DELIMITER,
      &targetPathsList);

   for(StringListIter iter = targetPathsList.begin();
       iter != targetPathsList.end();
       /* conditional iter inc inside loop */)
   {
      Path currentPath(StringTk::trim(*iter) );

      if(currentPath.empty())
      { // empty path => remove
         iter = targetPathsList.erase(iter);
         continue;
      }

      *iter = currentPath.str(); // normalize path

      if(!currentPath.absolute() ) /* (check to avoid problems after chdir) */
         throw InvalidConfigException("Path to storage target directory must be absolute: " +
            *iter);

      if(!cfg->getStoreAllowFirstRunInit() &&
         !StorageTk::checkStorageFormatFileExists(*iter) )
         throw InvalidConfigException(std::string("Found uninitialized storage target directory "
            "and initialization has been disabled: ") + *iter);

      if(primaryPath.empty() )
         primaryPath = *iter;

      iter++;
   }

   // check whether there are any targetPaths left after trimming in the loop above
   if(targetPathsList.empty() )
      throw InvalidConfigException("No storage target directory defined");
}

/**
 * initialize the quotaBlockDevice which contains quota related information about the block devices
 *
 * Note: requires initialized targetPaths
 * Note: No locking needed because it is only used in the constructor (no parallel access).
 */
void StorageTargets::initQuotaBlockDevices()
{
   supportForInodeQuota = QuotaInodeSupport_UNKNOWN;

   for (StorageTargetDataMapIter iter = storageTargetDataMap.begin();
      iter != storageTargetDataMap.end(); iter++)
   {
      (iter->second).quotaBlockDevice = QuotaBlockDevice::getBlockDeviceOfTarget(
         (iter->second).targetPath, iter->first);

      supportForInodeQuota = QuotaBlockDevice::updateQuotaInodeSupport(supportForInodeQuota,
         (iter->second).quotaBlockDevice.getFsType() );
   }
}

/**
 * Decide whether a resync is needed for any of the storage targets, and set its targetState to
 * TargetState_RESYNCING if so.
 */
void StorageTargets::decideResync(const TargetStateMap& statesFromMgmtd,
   TargetStateMap& outLocalStateChanges)
{
   const char* logContext = "Decide resync";

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* mirrorBuddyGroupMapper = app->getMirrorBuddyGroupMapper();


   // Make a map of targetID->Iterator on statesFromMgmtd containing only the local target
   // to make lookup under lock quicker.
   typedef std::map<uint16_t, TargetStateMapCIter> TargetStateIterMap;
   TargetStateIterMap stateFromMgmtdIterMap;
   // First we make a list of local target IDs (so later, when we iterate over all local states
   // in the "main loop" holding a read lock, we don't have to look them up in the (potentially
   // big) statesFromMgmtd, only in the smaller stateFromMgmtdIterMap.
   UInt16List localTargetIDs;
   {
      SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

      for (StorageTargetDataMapCIter localTargetIt = this->storageTargetDataMap.begin();
           localTargetIt != this->storageTargetDataMap.end(); ++localTargetIt)
         localTargetIDs.push_back(localTargetIt->first);

      safeLock.unlock(); // U N L O C K
   }

   // Fill the map (only operating on local data structures -> no lock needed).
   for (UInt16ListConstIter localTargetIDIt = localTargetIDs.begin();
        localTargetIDIt != localTargetIDs.end(); ++localTargetIDIt)
   {
      const uint16_t targetID = *localTargetIDIt;

      TargetStateMapCIter targetStateMapIter = statesFromMgmtd.find(targetID);
      if  (targetStateMapIter == statesFromMgmtd.end() )
      {
         LOG_DEBUG(logContext, Log_DEBUG, "Local target " + StringTk::uintToStr(targetID) +
            " not found in management node state report.");
         continue;
      }

      stateFromMgmtdIterMap.insert(std::pair<uint16_t, TargetStateMapCIter>(
         targetID, targetStateMapIter) );
   }

   const bool anyTargetHasTimeout = this->targetOfflineWait.anyTargetHasTimeout();

   // Now we can iterate over the storageTargetDataMap and do the actual resync decision.
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   for (StorageTargetDataMapIter storageTargetDataIt = this->storageTargetDataMap.begin();
        storageTargetDataIt != this->storageTargetDataMap.end(); ++storageTargetDataIt)
   {
      uint16_t targetID = storageTargetDataIt->first;
      StorageTargetData& targetData = storageTargetDataIt->second;

      TargetStateIterMap::const_iterator newStateFromMgmtdIt = stateFromMgmtdIterMap.find(targetID);
      if (newStateFromMgmtdIt == stateFromMgmtdIterMap.end() )
         continue; // Don't need to log - has been logged above.

      // Note: This still references the state stored in the inOutStatesFromMgmtd map.
      const CombinedTargetState& newStateFromMgmtd = newStateFromMgmtdIt->second->second;

      MirrorBuddyState buddyState = BuddyState_UNMAPPED;

      {
         SafeRWLock targetBuddyGroupSafeLock(&targetBuddyGroupMapLock, SafeRWLock_READ); // L O C K

         TargetBuddyGroupMapCIter bgIDIter = this->targetBuddyGroupMap.find(targetID);

         if (bgIDIter != this->targetBuddyGroupMap.end() )
            buddyState = mirrorBuddyGroupMapper->getBuddyState(targetID, bgIDIter->second);

         targetBuddyGroupSafeLock.unlock(); // U N L O C K
      }

      if (buddyState == BuddyState_UNMAPPED) // Unmapped targets can never be resyncing.
      {
         targetData.consistencyState = TargetConsistencyState_GOOD;

         outLocalStateChanges.insert(TargetStateMapVal(targetID,
            CombinedTargetState(TargetReachabilityState_ONLINE, TargetConsistencyState_GOOD) ) );

         // Setting the cleanShutdown flag has two purposes here:
         // * An unmapped target does not have a buddy to resync from, therefore even if it's not
         //   clean nothing can be done about it.
         // * A newly created target has to be marked as "clean" before it can join any mirror buddy
         //   group.
         targetData.cleanShutdown = true;

         continue;
      }

      // BAD targets will stay BAD unless server is restarted.
      if(targetData.consistencyState == TargetConsistencyState_BAD)
         continue;

      const TargetConsistencyState oldState = targetData.consistencyState;

      const bool isResyncing =
         newStateFromMgmtd.consistencyState == TargetConsistencyState_NEEDS_RESYNC;
      const bool isBad = newStateFromMgmtd.consistencyState == TargetConsistencyState_BAD;
      const bool isClean = targetData.cleanShutdown;

      // cleanShutdown should only be handled once at startup, so it is reset here after its value
      // has been copied.
      targetData.cleanShutdown = true;

      if (anyTargetHasTimeout && this->targetOfflineWait.targetHasTimeout(targetID) )
      {
         // Targets with a waiting-for-offline timeout will always be NEEDS_RESYNC.
         targetData.consistencyState = TargetConsistencyState_NEEDS_RESYNC;
         outLocalStateChanges.insert(TargetStateMapVal(targetID, CombinedTargetState(
            TargetReachabilityState_ONLINE, TargetConsistencyState_NEEDS_RESYNC) ) );
      }
      else
      if (oldState == TargetConsistencyState_NEEDS_RESYNC)
      {
         // If our local state is already RESYNCING, this state can only be left
         // when our primary tells us the resync is finished.
         targetData.consistencyState = TargetConsistencyState_NEEDS_RESYNC;
         outLocalStateChanges.insert(TargetStateMapVal(targetID, CombinedTargetState(
            TargetReachabilityState_ONLINE, TargetConsistencyState_NEEDS_RESYNC) ) );
      }
      else
      if(!isClean || isResyncing || isBad )
      {
         // This condition implements the following decision graph:
         // RESYNC? ->o                                    ,-->o-->(clean shutdown)--> NO
         //           |\                 ,->(!resyncing)--'     `-(dirty)--.
         //           \ `-------------->o->(resyncing)---------------------,`---> YES
         //            `------->(bad)-------------------------------------'

         targetData.consistencyState = TargetConsistencyState_NEEDS_RESYNC;
         outLocalStateChanges.insert(TargetStateMapVal(targetID, CombinedTargetState(
            TargetReachabilityState_ONLINE, TargetConsistencyState_NEEDS_RESYNC) ) );
      }
      else
      {
         // If mgmtd reports the target is (P)OFFLINE, then the storage server knows better and we
         // set the target to GOOD / ONLINE. Otherwise we accept the state reported by the mgmtd.
         if( (newStateFromMgmtd.reachabilityState == TargetReachabilityState_OFFLINE)
            || (newStateFromMgmtd.reachabilityState == TargetReachabilityState_POFFLINE) )
         {
            targetData.consistencyState = TargetConsistencyState_GOOD;
            outLocalStateChanges.insert(TargetStateMapVal(targetID, CombinedTargetState(
               TargetReachabilityState_ONLINE, TargetConsistencyState_GOOD) ) );

         }
         else
         {
            targetData.consistencyState = newStateFromMgmtd.consistencyState;
            outLocalStateChanges.insert(TargetStateMapVal(targetID, CombinedTargetState(
               TargetReachabilityState_ONLINE, newStateFromMgmtd.consistencyState) ) );
         }
      }

      if ( targetData.consistencyState != oldState
        || targetData.consistencyState != newStateFromMgmtd.consistencyState
        || newStateFromMgmtd.reachabilityState != TargetReachabilityState_ONLINE)
      {
         LOG_DEBUG(logContext, Log_DEBUG, "Target " + StringTk::uintToStr(targetID)
            + "; State (from management node): " + TargetStateStore::stateToStr(newStateFromMgmtd)
            + "; resyncing: " + (isResyncing ? "yes" : "no")
            + "; clean: " + (isClean ? "yes" : "no")
            + "; bad: " + (isBad ? "yes" : "no") + "."
         );

         LogContext(logContext).log(Log_NOTICE, "Target " + StringTk::uintToStr(targetID)
            + ": Setting new target state: "
            + TargetStateStore::stateToStr(targetData.consistencyState)
            + " (old state: " + TargetStateStore::stateToStr(oldState) + ")" );
      }
   }

   safeLock.unlock(); // U N L O C K
}


/**
 * Checks all local primary buddies whether there is a buddyneedsresync file, and if so, sets the
 * buddy's consistency state on the mgmtd to NEEDS_RESYNC. Afterwards, checks the all the target
 * state of the secondary buddies belonging to local primaries: if any secondary is ONLINE and
 * NEEDS_RESYNC, a ResyncJob is started.
 */
void StorageTargets::checkBuddyNeedsResync()
{
   const char* logContext = "Check buddy needs resync";
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* mirrorBuddyGroupMapper = app->getMirrorBuddyGroupMapper();
   TargetStateStore* targetStateStore = app->getTargetStateStore();
   BuddyResyncer* buddyResyncer = app->getBuddyResyncer();

   // For targets where primary is on this storage server:
   // Maps targetID->targetPath.
   TargetPathMap targetPathMap;
   // Maps primary->secondary. (Note: Not the same as TargetBuddyGroupMap, which maps target->group)
   typedef std::map<uint16_t, uint16_t> BuddyGroupMap;
   typedef BuddyGroupMap::const_iterator BuddyGroupMapCIter;
   typedef BuddyGroupMap::value_type BuddyGroupMapVal;
   BuddyGroupMap buddyGroupMap;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   for (StorageTargetDataMapIter targetDataIt = this->storageTargetDataMap.begin();
        targetDataIt != this->storageTargetDataMap.end(); ++targetDataIt)
   {
      uint16_t targetID = targetDataIt->first;

      targetPathMap.insert(TargetPathMapVal(targetID, targetDataIt->second.targetPath) );

      bool isPrimary;
      uint16_t buddyTargetID = mirrorBuddyGroupMapper->getBuddyTargetID(targetID, &isPrimary);

      if (isPrimary)
         buddyGroupMap.insert(BuddyGroupMapVal(targetID, buddyTargetID) );
   }

   safeLock.unlock(); // U N L O C K

   // Now, check all the primary targets for the existence of a buddyneedsresync file.
   for (TargetPathMapCIter targetPathIter = targetPathMap.begin();
        targetPathIter != targetPathMap.end(); ++targetPathIter)
   {
      const uint16_t targetID = targetPathIter->first;

      bool isPrimary;
      const uint16_t buddyTargetID = mirrorBuddyGroupMapper->getBuddyTargetID(targetID, &isPrimary);

      if (!isPrimary)
         continue;

      const bool buddyNeedsResyncFileExists = getBuddyNeedsResyncByPath(targetPathIter->second);

      if (buddyNeedsResyncFileExists)
      {
         LOG_DEBUG(logContext, Log_DEBUG, "buddyneedsresync file found for target "
            + StringTk::uintToStr(targetID) );

         CombinedTargetState state = CombinedTargetState(TargetReachabilityState_ONLINE,
            TargetConsistencyState_NEEDS_RESYNC);
         targetStateStore->getState(buddyTargetID, state);

         // Only send message if buddy was still reported as GOOD before (otherwise the mgmtd
         // already knows it needs a resync, or it's BAD and shouldn't be resynced anyway.
         if (state.consistencyState == TargetConsistencyState_GOOD)
         {
            setBuddyNeedsResyncState(buddyTargetID, true, true);

            LogContext(logContext).log(Log_NOTICE, "Set needs-resync state for buddy target "
               + StringTk::uintToStr(buddyTargetID) );
         }
      }
   }

   // And check all secondary targets whether they are in NEEDS_RESYNC state.
   for (BuddyGroupMapCIter buddyGroupIter = buddyGroupMap.begin();
        buddyGroupIter != buddyGroupMap.end(); ++buddyGroupIter)
   {
      const uint16_t targetID = buddyGroupIter->first;
      const uint16_t buddyTargetID = buddyGroupIter->second;

      TargetConsistencyState targetConsistencyState = TargetConsistencyState_BAD; // avoid warning
      if (!getState(targetID, targetConsistencyState) )
      {
         LOG_DEBUG(logContext, Log_DEBUG, "Target state invalid for target ID "
            + StringTk::uintToStr(targetID) );
         continue;
      }

      // If primary is not GOOD, don't start a resync (wait until InternodeSyncer sets the primary
      // to GOOD again).
      if (targetConsistencyState != TargetConsistencyState_GOOD)
      {
         LOG_DEBUG(logContext, Log_DEBUG, "Target state is not good, won't check buddy target state"
            "; target ID " + StringTk::uintToStr(targetID) );
         continue;
      }

      CombinedTargetState buddyTargetState;
      if (!targetStateStore->getState(buddyTargetID, buddyTargetState) )
      {
         LOG_DEBUG(logContext, Log_DEBUG, "Buddy target state invalid for target ID "
            + StringTk::uintToStr(buddyTargetID) );
         continue;
      }

      if (buddyTargetState ==
          CombinedTargetState(TargetReachabilityState_ONLINE, TargetConsistencyState_NEEDS_RESYNC) )
      {
         FhgfsOpsErr resyncRes = buddyResyncer->startResync(targetID);

         if (resyncRes == FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).log(Log_WARNING, "Target: " + StringTk::uintToStr(targetID)
               + " start buddy resync job.");
         }
         else
         {
            LOG_DEBUG(logContext, Log_DEBUG, "Target: " + StringTk::uintToStr(targetID)
               + " start buddy resync job failed ("
               + FhgfsOpsErrTk::toErrString(resyncRes) + ")");
         }
      }
   }
}


/**
 * Sets the NEEDS_RESYNC state on the mgmtd for a target or its buddy.
 * @param targetID ID of the target for whose buddy the state is to be set
 *                 (example: A buddy group consists of targets 5 and 6. If targetID=5, then the
 *                 resync state will be set for target 6)
 * @param needsResync If true: Set state to NEEDS_RESYNC. If false: Set state to GOOD.
 * @param targetIDIsBuddy: If false: buddy target ID resolution will be done as described above.
 *                         If true: Target ID is treated as buddy target ID - therefore the state
 *                         will be set for the target itself, not for its buddy.
 */
void StorageTargets::setBuddyNeedsResyncState(uint16_t targetID, bool needsResync,
   bool targetIDIsBuddy)
{
   const char* logContext = "Set buddy needs resync state";

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   MirrorBuddyGroupMapper* mirrorBuddyGroupMapper= app->getMirrorBuddyGroupMapper();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
   {
      LogContext(logContext).logErr("Management node not defined.");
      return;
   }

   TargetConsistencyState stateToSet = needsResync
      ? TargetConsistencyState_NEEDS_RESYNC
      : TargetConsistencyState_GOOD;

   uint16_t buddyTargetID = targetIDIsBuddy
      ? targetID
      : mirrorBuddyGroupMapper->getBuddyTargetID(targetID, NULL);

   UInt16List targetIDList(1, buddyTargetID);
   UInt8List stateList(1, stateToSet);

   SetTargetConsistencyStatesMsg msg(NODETYPE_Storage, &targetIDList, &stateList, false);

   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   bool sendRes = MessagingTk::requestResponse(*mgmtNode, &msg,
      NETMSGTYPE_SetTargetConsistencyStatesResp, &respBuf, &respMsg);

   if (!sendRes)
   {
      LogContext(logContext).log(Log_WARNING, "Could not reach management node when trying to set "
         "needs-resync state for target " + StringTk::uintToStr(buddyTargetID) );
   }
   else
   {
      SetTargetConsistencyStatesRespMsg* respMsgCast =
         (SetTargetConsistencyStatesRespMsg*)respMsg;

      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
         LogContext(logContext).log(Log_CRITICAL, "Management node did not accept target states.");

      SAFE_FREE(respBuf);
      SAFE_DELETE(respMsg);
   }
}

/**
 * Locks all targetPaths by creating a lock file for each target.
 *
 * Note: Don't forget to call unlockTargets() later.
 */
void StorageTargets::lockTargetPaths() throw(InvalidConfigException)
{
   for(StringListIter iter = targetPathsList.begin(); iter != targetPathsList.end(); iter++)
   {
      int lockFD = StorageTk::lockWorkingDirectory(*iter);
      if(lockFD == -1)
         throw InvalidConfigException("Invalid storage directory: locking failed");

      targetLockFDs.push_back(lockFD);
   }
}

void StorageTargets::unlockTargetPaths() throw(InvalidConfigException)
{
   for(IntListIter iter = targetLockFDs.begin();
       iter != targetLockFDs.end();
       /* iter inc'ed inside loop */)
   {
      if(*iter != -1)
         StorageTk::unlockWorkingDirectory(*iter);

      iter = targetLockFDs.erase(iter);
   }
}

/**
 * Initialize subdirectory structure of each target dir.
 */
void StorageTargets::prepareTargetDirs(Config* cfg) throw(InvalidConfigException)
{
   // walk over all target dirs and create subdirs, format file, ...

   mode_t targetMkdirMode = STORAGETARGETS_TARGETDIR_MKDIR_MODE;
   mode_t chunkMkDirMode  = STORAGETARGETS_CHUNKDIR_MKDIR_MODE;

   for(StringListIter iter = targetPathsList.begin(); iter != targetPathsList.end(); iter++)
   {
      Path currentPath(*iter);

      if(!StorageTk::createPathOnDisk(currentPath, false, &targetMkdirMode) )
         throw InvalidConfigException("Unable to create storage directory: " + *iter);

      // storage format file
      if(!StorageTk::createStorageFormatFile(*iter, STORAGETK_FORMAT_CURRENT_VERSION) )
         throw InvalidConfigException("Unable to create storage format file in: " + *iter);

      StorageTk::checkAndUpdateStorageFormatFile(*iter, STORAGETK_FORMAT_MIN_VERSION,
         STORAGETK_FORMAT_CURRENT_VERSION);

      // chunks directory
      Path chunksPath(*iter + "/" CONFIG_CHUNK_SUBDIR_NAME);

      if(!StorageTk::createPathOnDisk(chunksPath, false, &chunkMkDirMode) )
         throw InvalidConfigException("Unable to create chunks directory: " +
            chunksPath.str() );

      // chunks subdirs
      if(cfg->getStoreInitHashDirs() )
         StorageTk::initHashPaths(chunksPath,
            CONFIG_CHUNK_LEVEL1_SUBDIR_NUM, CONFIG_CHUNK_LEVEL1_SUBDIR_NUM);

      // buddy mirror directory
      Path mirrorPath(*iter + "/" CONFIG_BUDDYMIRROR_SUBDIR_NAME);
      if(!StorageTk::createPathOnDisk(mirrorPath, false, &chunkMkDirMode) )
         throw InvalidConfigException("Unable to create buddy mirror directory: " +
            mirrorPath.str() );

   } // end of for(targetPathsList) loop

}

/**
 * Add (or overwrite) a targetNumID/storagePath pair
 */
bool StorageTargets::addTargetDir(uint16_t targetNumID, std::string storagePathStr)
{
   const char* logContext = "StorageTargets add target";

   Path storagePath(storagePathStr); // to get a normalized path string
   bool retVal = true;

   std::string chunkPath;
   std::string mirrorPath;
   int chunkFD;
   int mirrorFD;
   bool sessionFileExists;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   // plain chunks path

   chunkPath = storagePathStr + "/" CONFIG_CHUNK_SUBDIR_NAME;
   chunkFD = open(chunkPath.c_str(), O_RDONLY | O_DIRECTORY);
   if (chunkFD == -1)
   {
      LogContext(logContext).logErr("Failed to open directory: " + chunkPath + ". SysErr: " +
         System::getErrString() );
      retVal = false;

      // don't even try mirrors and buddy mirrors
   }
   else
   { // successfully opened
      StorageTargetData& storageTargetData = storageTargetDataMap[targetNumID];
      storageTargetData.chunkFD = chunkFD;

      // mirror chunks path

      mirrorPath = storagePathStr + "/" CONFIG_BUDDYMIRROR_SUBDIR_NAME;
      mirrorFD = open(mirrorPath.c_str(), O_RDONLY | O_DIRECTORY);
      if (mirrorFD == -1)
      {
         LogContext(logContext).logErr("Failed to open directory: " + mirrorPath + ". SysErr: " +
               System::getErrString() );
         retVal = false;

         // cleanup
         close(chunkFD);
         storageTargetData.chunkFD = -1;

         goto unlock_and_exit; // don't even try buddy mirrors
      }
      else
      {  // successfully opened
         storageTargetData.mirrorFD = mirrorFD;
      }

      storageTargetData.targetPath = storagePath.str();

      // Check if target was cleanly shut down.
      sessionFileExists = StorageTk::checkSessionFileExists(storagePath.str());
      storageTargetData.cleanShutdown = sessionFileExists;
   }

unlock_and_exit:
   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Remove a numID/storagePath pair by numID
 *
 * @return true if such a numID existed
 */
bool StorageTargets::removeTargetDir(uint16_t targetNumID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool retVal = removeTargetDirUnlocked(targetNumID);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Remove a numID/storagePath pair by numID
 *
 * @return true if such a numID existed
 */
bool StorageTargets::removeTargetDirUnlocked(uint16_t targetNumID)
{
   int chunkFD = getChunkFDUnlocked(targetNumID);
   if (chunkFD != -1)
      close(chunkFD); // ignore the error, we don't know what to do with it anyway

   int mirrorFD = getMirrorFDUnlocked(targetNumID);
   if (mirrorFD != -1)
      close(mirrorFD); // ignore the error, we don't know what to do with it anyway

   size_t numErased = storageTargetDataMap.erase(targetNumID);
   bool retVal = (numErased) ? true : false;

   return retVal;
}

/**
 * Remove a numID/storagePath pair by storagePath and the corresponding quotaBlockDevice
 *
 * @param removeAll false by default, if true storagePathStr is ignored
 * @return true if such a storagePathStr existed
 */
bool StorageTargets::removeTargetDir(std::string storagePathStr, bool removeAll)
{
   bool pathExisted = false;

   Path storagePath(storagePathStr); // to get a normalized path string
   std::string normalizedStoragePath(storagePath.str() );

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   // walk over all targetPaths and compare storage paths
   for(StorageTargetDataMapIter iter = storageTargetDataMap.begin();
      iter != storageTargetDataMap.end(); iter++)
   {
      if(removeAll || (iter->second).targetPath == normalizedStoragePath)
      {
         uint16_t targetNumID = iter->first;

         removeTargetDirUnlocked(targetNumID);

         pathExisted = true;
         break;
      }
   }

   safeLock.unlock(); // U N L O C K

   return pathExisted;
}

/**
 * Handles TargetState changes and carries out necessary actions (notify mgmtd, start resync, ...).
 */
void StorageTargets::handleTargetStateChange(uint16_t targetID, TargetConsistencyState newState)
{
   const char* logContext("Handle target state change");

   App* app = Program::getApp();
   InternodeSyncer* internodeSyncer = app->getInternodeSyncer();

   // Inform Mgmtd about state change.
   LogContext(logContext).log(Log_DEBUG, "Notifying management node about of change. targetID: "
      + StringTk::uintToStr(targetID) + "; new state: " + TargetStateStore::stateToStr(newState) );

   if (unlikely(this->targetOfflineWait.anyTargetHasTimeout() &&
       this->targetOfflineWait.targetHasTimeout(targetID) ) )
      internodeSyncer->publishTargetState(targetID, newState);
}

void StorageTargets::generateTargetInfoList(const UInt16List& targetIDs,
   StorageTargetInfoList& outTargetInfoList)
{
   const char* logContext = "Generate storage target info list";

   for (UInt16ListConstIter iter = targetIDs.begin(); iter != targetIDs.end(); iter++)
   {
      uint16_t targetID = *iter;
      std::string targetPathStr;
      int64_t sizeTotal = 0;
      int64_t sizeFree = 0;
      int64_t inodesTotal = 0;
      int64_t inodesFree = 0;
      TargetConsistencyState targetState = TargetConsistencyState_BAD;
      bool gotTargetPath;
      bool getStateRes;

      gotTargetPath = this->getPath(targetID, &targetPathStr);

      if(unlikely(!gotTargetPath) )
      { // unknown targetID
         LogContext(logContext).logErr("Unknown targetID: " + StringTk::uintToStr(targetID) );

         targetPathStr.clear();
         // Leave targetState as "BAD".

         goto append_info;
      }

      getStatInfo(targetPathStr, &sizeTotal, &sizeFree, &inodesTotal, &inodesFree);

      getStateRes = this->getState(targetID, targetState);

      if(unlikely(!getStateRes) )
         LogContext(logContext).logErr("Target state unknown; "
            "targetID: " + StringTk::uintToStr(targetID) );


   append_info:
      StorageTargetInfo targetInfo(targetID, targetPathStr, sizeTotal, sizeFree, inodesTotal,
         inodesFree, targetState);
      outTargetInfoList.push_back(targetInfo);
   }

}

/**
 * note: sets "-1" for all out-values if statfs failed.
 */
void StorageTargets::getStatInfo(std::string& targetPathStr, int64_t* outSizeTotal,
   int64_t* outSizeFree, int64_t* outInodesTotal, int64_t* outInodesFree)
{
   const char* logContext = "Get storage target info";

   bool statSuccess = StorageTk::statStoragePath(targetPathStr, outSizeTotal, outSizeFree,
      outInodesTotal, outInodesFree);

   if(unlikely(!statSuccess) )
   { // error
      LogContext(logContext).logErr("Unable to statfs() storage path: " + targetPathStr +
         " (SysErr: " + System::getErrString() );

      *outSizeTotal = -1;
      *outSizeFree = -1;
      *outInodesTotal = -1;
      *outInodesFree = -1;
   }

   // read and use value from manual free space override file (if it exists)
   StorageTk::statStoragePathOverride(targetPathStr, outSizeFree, outInodesFree);
}

/**
 * Add a targetOfflineWait timeout to each target which is "NEEDS_RESYNC" at the moment.
 * Intended to be called at startup so the targets that need a resync to not report a state to the
 * mgmtd until they are surely OFFLINEd.
 */
void StorageTargets::addStartupTimeout()
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* mirrorBuddyGroupMapper = app->getMirrorBuddyGroupMapper();

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   for (StorageTargetDataMapCIter targetDataIter = storageTargetDataMap.begin();
        targetDataIter != storageTargetDataMap.end(); ++targetDataIter)
   {
      uint16_t targetID = targetDataIter->first;

      if (targetDataIter->second.consistencyState == TargetConsistencyState_NEEDS_RESYNC
          && mirrorBuddyGroupMapper->getBuddyState(targetID) == BuddyState_PRIMARY)
         targetOfflineWait.addTarget(targetID);
   }

   safeLock.unlock(); // U N L O C K
}

void StorageTargets::syncBuddyGroupMap(const UInt16List& buddyGroupIDs, const UInt16List& primaryTargetIDs,
   const UInt16List& secondaryTargetIDs)
{
   TargetBuddyGroupMap newTargetBuddyGroupMap;

   for (ZipConstIterRange<UInt16List, UInt16List, UInt16List>
        iter(buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs);
        !iter.empty(); ++iter)
   {
      uint16_t groupID = *iter()->first;
      newTargetBuddyGroupMap.insert(std::pair<uint16_t, uint16_t>(*iter()->second, groupID) );
      newTargetBuddyGroupMap.insert(std::pair<uint16_t, uint16_t>(*iter()->third, groupID) );
   }

   SafeRWLock safeLock(&targetBuddyGroupMapLock, SafeRWLock_WRITE); // L O C K
   newTargetBuddyGroupMap.swap(this->targetBuddyGroupMap);
   safeLock.unlock(); // U N L O C K
}

/**
 * Fill a storageTargetDataMap with the contents of the three lists passed.
 * targetIDs, reachabilityStates and consistencyStates is interpreted as a 1:1 mapping.
 */
void StorageTargets::fillTargetStateMap(const UInt16List& targetIDs,
   const UInt8List& reachabilityStates, const UInt8List& consistencyStates,
   TargetStateMap& outStateMap)
{
   for (ZipConstIterRange<UInt16List, UInt8List, UInt8List>
        iter(targetIDs, reachabilityStates, consistencyStates);
        !iter.empty(); ++iter)
   {
      outStateMap.insert(TargetStateMapVal(*iter()->first, CombinedTargetState(
            static_cast<TargetReachabilityState>(*iter()->second),
            static_cast<TargetConsistencyState>(*iter()->third)
         ) ) );
   }
}

void StorageTargets::updateTargetStateLists(const TargetStateMap& stateMap, const UInt16List& targetIDs,
   UInt8List& outReachabilityStates, UInt8List& outConsistencyStates)
{
   UInt16ListConstIter targetIDIter = targetIDs.begin();
   UInt8ListIter reachabilityStateIter = outReachabilityStates.begin();
   UInt8ListIter consistencyStateIter = outConsistencyStates.begin();

   for ( ; targetIDIter != targetIDs.end();
      ++targetIDIter, ++reachabilityStateIter, ++consistencyStateIter)
   {
      TargetStateMapCIter newStateIter = stateMap.find(*targetIDIter);

      if (newStateIter == stateMap.end() )
         continue;

      *reachabilityStateIter = newStateIter->second.reachabilityState;
      *consistencyStateIter = newStateIter->second.consistencyState;
   }
}

TargetConsistencyStateVec StorageTargets::getTargetConsistencyStates(UInt16Vector targetIDs)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   TargetConsistencyStateVec states;
   states.reserve(targetIDs.size());

   for (auto it = targetIDs.cbegin(); it != targetIDs.cend(); ++it)
   {
      auto dataIt = storageTargetDataMap.find(*it);

      if (dataIt == storageTargetDataMap.end())
         states.push_back(TargetConsistencyState_BAD);
      else
         states.push_back((dataIt->second).consistencyState);
   }

   return states;
}
