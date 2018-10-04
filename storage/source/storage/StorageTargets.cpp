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

#include <boost/lexical_cast.hpp>


#define STORAGETARGETS_TARGETDIR_MKDIR_MODE (S_IRWXU)
#define STORAGETARGETS_CHUNKDIR_MKDIR_MODE (S_IRWXU | S_IRWXG | S_IRWXO)

#define BUDDY_NEEDS_RESYNC_FILENAME        ".buddyneedsresync"
#define LAST_BUDDY_COMM_TIMESTAMP_FILENAME ".lastbuddycomm"


StorageTarget::StorageTarget(Path path, uint16_t targetID, TimerQueue& timerQueue,
      NodeStoreServers& mgmtNodes, MirrorBuddyGroupMapper& buddyGroupMapper):
   path(std::move(path)), id(targetID),
   buddyNeedsResyncFile((this->path / BUDDY_NEEDS_RESYNC_FILENAME).str(), S_IRUSR | S_IWUSR),
   lastBuddyCommFile((this->path / LAST_BUDDY_COMM_TIMESTAMP_FILENAME).str(), S_IRUSR | S_IWUSR),
   timerQueue(timerQueue), mgmtNodes(mgmtNodes),
   buddyGroupMapper(buddyGroupMapper), buddyResyncInProgress(false),
   consistencyState(TargetConsistencyState_GOOD), cleanShutdown(false)
{
   const auto chunkPath = this->path / CONFIG_CHUNK_SUBDIR_NAME;
   chunkFD = FDHandle(open(chunkPath.str().c_str(), O_RDONLY | O_DIRECTORY));
   if (!chunkFD.valid())
      throw std::system_error(errno, std::system_category(), chunkPath.str());

   // mirror chunks path

   const auto mirrorPath = this->path / CONFIG_BUDDYMIRROR_SUBDIR_NAME;
   mirrorFD = FDHandle(open(mirrorPath.str().c_str(), O_RDONLY | O_DIRECTORY));
   if (!mirrorFD.valid())
      throw std::system_error(errno, std::system_category(), mirrorPath.str());

   quotaBlockDevice = QuotaBlockDevice::getBlockDeviceOfTarget(this->path.str(), targetID);

   if (buddyNeedsResyncFile.read().get_value_or(0) & BUDDY_RESYNC_UNACKED_FLAG)
   {
      setBuddyNeedsResyncEntry = timerQueue.enqueue(std::chrono::seconds(0), [this] {
         retrySetBuddyNeedsResyncComm();
      });
   }
}

void StorageTarget::prepareTargetDir(const Path& path)
{
   mode_t targetMkdirMode = STORAGETARGETS_TARGETDIR_MKDIR_MODE;
   mode_t chunkMkDirMode  = STORAGETARGETS_CHUNKDIR_MKDIR_MODE;

   if (!StorageTk::createPathOnDisk(path, false, &targetMkdirMode))
      throw InvalidConfigException("Unable to create storage directory: " + path.str());

   // storage format file
   if (!StorageTk::createStorageFormatFile(path.str(), STORAGETK_FORMAT_CURRENT_VERSION))
      throw InvalidConfigException("Unable to create storage format file in: " + path.str());

   StorageTk::loadAndUpdateStorageFormatFile(path.str(), STORAGETK_FORMAT_MIN_VERSION,
      STORAGETK_FORMAT_CURRENT_VERSION);

   // chunks directory
   Path chunksPath = path / CONFIG_CHUNK_SUBDIR_NAME;

   if (!StorageTk::createPathOnDisk(chunksPath, false, &chunkMkDirMode))
      throw InvalidConfigException("Unable to create chunks directory: " + chunksPath.str());

   // buddy mirror directory
   Path mirrorPath = path / CONFIG_BUDDYMIRROR_SUBDIR_NAME;
   if (!StorageTk::createPathOnDisk(mirrorPath, false, &chunkMkDirMode))
      throw InvalidConfigException("Unable to create buddy mirror directory: " + mirrorPath.str());
}

bool StorageTarget::isTargetDir(const Path& path)
{
   return StorageTk::checkStorageFormatFileExists(path.str());
}

void StorageTarget::setBuddyNeedsResync(bool needsResync)
{
   const RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   const auto oldState = buddyNeedsResyncFile.read().get_value_or(BUDDY_RESYNC_NOT_REQUIRED);
   const auto newState = needsResync
      ? BUDDY_RESYNC_REQUIRED_UNACKED
      : BUDDY_RESYNC_NOT_REQUIRED_UNACKED;

   // if the change has already been requested by some other thread, we should not request it
   // again - even if the change is unacked, as retrying immediately after a failed communication
   // attempt is not likely to be successful, we must handle externally started resyncs however,
   // which *do not* change the buddyneedsresync file contents but *do* use this mechanism to
   // communicate that a resync has finished and the buddy is good again - these only use us to
   // set the buddy to "needs no resync" though, so we can still skip setting needs-resync when that
   // is already pending.
   if (needsResync
         && (oldState & BUDDY_RESYNC_REQUIRED_FLAG) == (newState & BUDDY_RESYNC_REQUIRED_FLAG))
      return;

   // cancel any pending retries, we will send a message to mgmt anyway.
   if (setBuddyNeedsResyncEntry)
      setBuddyNeedsResyncEntry->cancel();

   buddyNeedsResyncFile.write(newState);

   if (!setBuddyNeedsResyncComm())
      LOG(GENERAL, CRITICAL, "Could not reach mgmt for state update, will retry.",
            ("primary target", id), ("buddyNeedsResync", needsResync));
}

bool StorageTarget::setBuddyNeedsResyncComm()
{
   // this is a timer callback. as such we must be prepared to deal with the fact that we were
   // cancelled *after* we were dequeued and started executing, but were blocked on the lock in
   // retrySetBuddyNeedsResyncComm. always reading the current state and sending that fixes this:
   // if the state is not unacked we can return without doing anything, and if it is nobody can
   // change it while we are using it.
   const auto state = buddyNeedsResyncFile.read().get_value_or(BUDDY_RESYNC_NOT_REQUIRED);
   const bool needsResync = state & BUDDY_RESYNC_REQUIRED_FLAG;

   if (!(state & BUDDY_RESYNC_UNACKED_FLAG))
      return true;

   const TargetConsistencyState stateToSet = needsResync
      ? TargetConsistencyState_NEEDS_RESYNC
      : TargetConsistencyState_GOOD;

   bool currentIsPrimary;
   const uint16_t buddyTargetID = buddyGroupMapper.getBuddyTargetID(id, &currentIsPrimary);

   // until mgmt handles resync decision, refuse to set a primary to needs-resync locally.
   if (!currentIsPrimary)
      return true;

   UInt16List targetIDList(1, buddyTargetID);
   UInt8List stateList(1, stateToSet);

   SetTargetConsistencyStatesMsg msg(NODETYPE_Storage, &targetIDList, &stateList, false);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNodes.referenceFirstNode(), msg,
         NETMSGTYPE_SetTargetConsistencyStatesResp);

   if (!respMsg)
   {
      setBuddyNeedsResyncEntry = timerQueue.enqueue(std::chrono::seconds(5), [this] {
         retrySetBuddyNeedsResyncComm();
      });
      return false;
   }

   auto* respMsgCast = (SetTargetConsistencyStatesRespMsg*)respMsg.get();

   if (respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
   {
      LOG(GENERAL, CRITICAL, "Management node did not accept target states.", buddyTargetID,
            needsResync);
      return true;
   }

   buddyNeedsResyncFile.write(state & ~BUDDY_RESYNC_UNACKED_FLAG);
   if (state & BUDDY_RESYNC_REQUIRED_FLAG)
      LOG(GENERAL, CRITICAL, "Marked secondary buddy for needed resync.", ("primary target", id));
   return true;
}

void StorageTarget::handleTargetStateChange()
{
   const auto newState = getConsistencyState();

   LOG(GENERAL, DEBUG, "Notifying management node of target state change.", id, newState);

   if (getOfflineTimeout())
      Program::getApp()->getInternodeSyncer()->publishTargetState(id, newState);
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

   std::map<uint16_t, uint16_t> targetBuddyGroupMap;
   {
      MirrorBuddyGroupMap groupMap;
      mirrorBuddyGroupMapper->getMirrorBuddyGroups(groupMap);

      for (const auto& group : groupMap)
      {
         targetBuddyGroupMap[group.second.firstTargetID] = group.first;
         targetBuddyGroupMap[group.second.secondTargetID] = group.first;
      }
   }


   // Make a map of targetID->Iterator on statesFromMgmtd containing only the local target
   // to make lookup under lock quicker.
   typedef std::map<uint16_t, TargetStateMapCIter> TargetStateIterMap;
   TargetStateIterMap stateFromMgmtdIterMap;
   // First we make a list of local target IDs (so later, when we iterate over all local states
   // in the "main loop" holding a read lock, we don't have to look them up in the (potentially
   // big) statesFromMgmtd, only in the smaller stateFromMgmtdIterMap.
   UInt16List localTargetIDs;
   {
      for (StorageTargetMapCIter localTargetIt = this->storageTargetMap.begin();
           localTargetIt != this->storageTargetMap.end(); ++localTargetIt)
         localTargetIDs.push_back(localTargetIt->first);
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

   // Now we can iterate over the storageTargetMap and do the actual resync decision.
   for (auto storageTargetIt = this->storageTargetMap.begin();
        storageTargetIt != this->storageTargetMap.end(); ++storageTargetIt)
   {
      uint16_t targetID = storageTargetIt->first;
      StorageTarget& targetData = *storageTargetIt->second;

      TargetStateIterMap::const_iterator newStateFromMgmtdIt = stateFromMgmtdIterMap.find(targetID);
      if (newStateFromMgmtdIt == stateFromMgmtdIterMap.end() )
         continue; // Don't need to log - has been logged above.

      // Note: This still references the state stored in the inOutStatesFromMgmtd map.
      const CombinedTargetState& newStateFromMgmtd = newStateFromMgmtdIt->second->second;

      MirrorBuddyState buddyState = BuddyState_UNMAPPED;

      {
         const auto bgIDIter = targetBuddyGroupMap.find(targetID);

         if (bgIDIter != targetBuddyGroupMap.end())
            buddyState = mirrorBuddyGroupMapper->getBuddyState(targetID, bgIDIter->second);
      }

      if (buddyState == BuddyState_UNMAPPED) // Unmapped targets can never be resyncing.
      {
         LOG_DBG(STATES, DEBUG,
               "Setting target to state good because it is not part of a mirror group.",
               ("oldState", TargetStateStore::stateToStr(targetData.getConsistencyState())),
               targetID);

         targetData.setConsistencyState(TargetConsistencyState_GOOD);

         outLocalStateChanges.insert(TargetStateMapVal(targetID,
            CombinedTargetState(TargetReachabilityState_ONLINE, TargetConsistencyState_GOOD) ) );

         // Setting the cleanShutdown flag has two purposes here:
         // * An unmapped target does not have a buddy to resync from, therefore even if it's not
         //   clean nothing can be done about it.
         // * A newly created target has to be marked as "clean" before it can join any mirror buddy
         //   group.
         targetData.setCleanShutdown(true);

         continue;
      }

      // BAD targets will stay BAD unless server is restarted.
      if (targetData.getConsistencyState() == TargetConsistencyState_BAD)
         continue;

      const TargetConsistencyState oldState = targetData.getConsistencyState();

      const bool isResyncing =
         newStateFromMgmtd.consistencyState == TargetConsistencyState_NEEDS_RESYNC;
      const bool isBad = newStateFromMgmtd.consistencyState == TargetConsistencyState_BAD;
      const bool isClean = targetData.getCleanShutdown();

      // cleanShutdown should only be handled once at startup, so it is reset here after its value
      // has been copied.
      targetData.setCleanShutdown(true);

      if (storageTargetIt->second->getOfflineTimeout())
      {
         // Targets with a waiting-for-offline timeout will always be NEEDS_RESYNC.
         targetData.setConsistencyState(TargetConsistencyState_NEEDS_RESYNC);
         outLocalStateChanges.insert(TargetStateMapVal(targetID, CombinedTargetState(
            TargetReachabilityState_ONLINE, TargetConsistencyState_NEEDS_RESYNC) ) );
      }
      else
      if (oldState == TargetConsistencyState_NEEDS_RESYNC)
      {
         // If our local state is already RESYNCING, this state can only be left
         // when our primary tells us the resync is finished.
         targetData.setConsistencyState(TargetConsistencyState_NEEDS_RESYNC);
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

         targetData.setConsistencyState(TargetConsistencyState_NEEDS_RESYNC);
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
            targetData.setConsistencyState(TargetConsistencyState_GOOD);
            outLocalStateChanges.insert(TargetStateMapVal(targetID, CombinedTargetState(
               TargetReachabilityState_ONLINE, TargetConsistencyState_GOOD) ) );

         }
         else
         {
            targetData.setConsistencyState(newStateFromMgmtd.consistencyState);
            outLocalStateChanges.insert(TargetStateMapVal(targetID, CombinedTargetState(
               TargetReachabilityState_ONLINE, newStateFromMgmtd.consistencyState) ) );
         }
      }

      if ( targetData.getConsistencyState() != oldState
        || targetData.getConsistencyState() != newStateFromMgmtd.consistencyState
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
            + TargetStateStore::stateToStr(targetData.getConsistencyState())
            + " (old state: " + TargetStateStore::stateToStr(oldState) + ")" );
      }
   }
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
   std::map<uint16_t, Path> targetPathMap;
   // Maps primary->secondary. (Note: Not the same as TargetBuddyGroupMap, which maps target->group)
   typedef std::map<uint16_t, uint16_t> BuddyGroupMap;
   typedef BuddyGroupMap::const_iterator BuddyGroupMapCIter;
   typedef BuddyGroupMap::value_type BuddyGroupMapVal;
   BuddyGroupMap buddyGroupMap;

   {
      for (auto targetDataIt = this->storageTargetMap.begin();
            targetDataIt != this->storageTargetMap.end(); ++targetDataIt)
      {
         uint16_t targetID = targetDataIt->first;

         targetPathMap.insert({targetID, targetDataIt->second->getPath()});

         bool isPrimary;
         uint16_t buddyTargetID = mirrorBuddyGroupMapper->getBuddyTargetID(targetID, &isPrimary);

         if (isPrimary)
            buddyGroupMap.insert(BuddyGroupMapVal(targetID, buddyTargetID) );
      }

   }

   // Now, check all the primary targets for the existence of a buddyneedsresync file.
   for (const auto& targetPath : targetPathMap)
   {
      const uint16_t targetID = targetPath.first;

      bool isPrimary;
      const uint16_t buddyTargetID = mirrorBuddyGroupMapper->getBuddyTargetID(targetID, &isPrimary);

      if (!isPrimary)
         continue;

      const bool buddyNeedsResync = storageTargetMap.at(targetID)->getBuddyNeedsResync();

      if (buddyNeedsResync)
      {
         LOG_DEBUG(logContext, Log_DEBUG, "buddyneedsresync indication found for target "
            + StringTk::uintToStr(targetID) );

         CombinedTargetState state = CombinedTargetState(TargetReachabilityState_ONLINE,
            TargetConsistencyState_NEEDS_RESYNC);
         targetStateStore->getState(buddyTargetID, state);

         // Only send message if buddy was still reported as GOOD before (otherwise the mgmtd
         // already knows it needs a resync, or it's BAD and shouldn't be resynced anyway.
         if (state.consistencyState == TargetConsistencyState_GOOD)
         {
            storageTargetMap.at(targetID)->setBuddyNeedsResync(true);

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

      const auto targetConsistencyState = storageTargetMap.at(targetID)->getConsistencyState();

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
               + boost::lexical_cast<std::string>(resyncRes) + ")");
         }
      }
   }
}

void StorageTargets::generateTargetInfoList(StorageTargetInfoList& outTargetInfoList)
{
   for (const auto& mapping : storageTargetMap)
   {
      uint16_t targetID = mapping.first;
      const auto& target = mapping.second;
      std::string targetPathStr;
      int64_t sizeTotal = 0;
      int64_t sizeFree = 0;
      int64_t inodesTotal = 0;
      int64_t inodesFree = 0;
      TargetConsistencyState targetState = TargetConsistencyState_BAD;

      targetPathStr = target->getPath().str();

      getStatInfo(targetPathStr, &sizeTotal, &sizeFree, &inodesTotal, &inodesFree);

      targetState = target->getConsistencyState();

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
 * Fill a storageTargetMap with the contents of the three lists passed.
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
