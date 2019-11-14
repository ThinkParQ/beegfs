#include <program/Program.h>
#include <components/buddyresyncer/BuddyResyncer.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/toolkit/PreallocatedFile.h>

#include "BuddyCommTk.h"

// TODO: file handling here is duplicated from storage::StorageTarget, unify the two at some point

#define BUDDY_NEEDS_RESYNC_FILENAME        ".buddyneedsresync"

namespace {

enum {
   BUDDY_RESYNC_UNACKED_FLAG = 1,
   BUDDY_RESYNC_REQUIRED_FLAG = 2,

   BUDDY_RESYNC_NOT_REQUIRED = 0,
   BUDDY_RESYNC_NOT_REQUIRED_UNACKED = BUDDY_RESYNC_UNACKED_FLAG,
   BUDDY_RESYNC_REQUIRED = BUDDY_RESYNC_REQUIRED_FLAG,
   BUDDY_RESYNC_REQUIRED_UNACKED = BUDDY_RESYNC_REQUIRED_FLAG | BUDDY_RESYNC_UNACKED_FLAG,
};

RWLock buddyNeedsResyncLock;
std::unique_ptr<PreallocatedFile<uint8_t>> buddyNeedsResyncFile;
boost::optional<TimerQueue::EntryHandle> setBuddyNeedsResyncEntry;

bool setBuddyNeedsResyncComm(Node& mgmtNode, const MirrorBuddyGroupMapper& bgm, TimerQueue& timerQ,
      NumNodeID localNodeID);

void retrySetBuddyNeedsResyncComm(Node& mgmtNode, const MirrorBuddyGroupMapper& bgm,
      TimerQueue& timerQ, const NumNodeID localNodeID)
{
   const RWLockGuard lock(buddyNeedsResyncLock, SafeRWLock_WRITE);
   setBuddyNeedsResyncComm(mgmtNode, bgm, timerQ, localNodeID);
}

void setBuddyNeedsResync(Node& mgmtNode, const MirrorBuddyGroupMapper& bgm, TimerQueue& timerQ,
      const NumNodeID localNodeID, const bool needsResync)
{
   const RWLockGuard lock(buddyNeedsResyncLock, SafeRWLock_WRITE);

   const auto oldState = buddyNeedsResyncFile->read().get_value_or(BUDDY_RESYNC_NOT_REQUIRED);
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

   buddyNeedsResyncFile->write(newState);

   if (!setBuddyNeedsResyncComm(mgmtNode, bgm, timerQ, localNodeID))
      LOG(GENERAL, CRITICAL, "Could not reach mgmt for state update, will retry.",
            ("buddyNeedsResync", needsResync));
}

bool getBuddyNeedsResync()
{
   const RWLockGuard lock(buddyNeedsResyncLock, SafeRWLock_READ);

   const auto state = buddyNeedsResyncFile->read().get_value_or(BUDDY_RESYNC_NOT_REQUIRED);
   return state & BUDDY_RESYNC_REQUIRED_FLAG;
}

bool setBuddyNeedsResyncComm(Node& mgmtNode, const MirrorBuddyGroupMapper& bgm, TimerQueue& timerQ,
      const NumNodeID localNodeID)
{
   // this is a timer callback. as such we must be prepared to deal with the fact that we were
   // cancelled *after* we were dequeued and started executing, but were blocked on the lock in
   // retrySetBuddyNeedsResyncComm. always reading the current state and sending that fixes this:
   // if the state is not unacked we can return without doing anything, and if it is nobody can
   // change it while we are using it.
   const auto state = buddyNeedsResyncFile->read().get_value_or(BUDDY_RESYNC_NOT_REQUIRED);
   const bool needsResync = state & BUDDY_RESYNC_REQUIRED_FLAG;

   if (!(state & BUDDY_RESYNC_UNACKED_FLAG))
      return true;

   const TargetConsistencyState stateToSet = needsResync
      ? TargetConsistencyState_NEEDS_RESYNC
      : TargetConsistencyState_GOOD;

   bool currentIsPrimary;
   const uint16_t buddyTargetID = bgm.getBuddyTargetID(localNodeID.val(), &currentIsPrimary);

   // until mgmt handles resync decision, refuse to set a primary to needs-resync locally.
   if (!currentIsPrimary)
   {
      buddyNeedsResyncFile->write(BUDDY_RESYNC_NOT_REQUIRED);
      return true;
   }

   UInt16List targetIDList(1, buddyTargetID);
   UInt8List stateList(1, stateToSet);

   SetTargetConsistencyStatesMsg msg(NODETYPE_Meta, &targetIDList, &stateList, false);

   const auto respMsg = MessagingTk::requestResponse(mgmtNode, msg,
         NETMSGTYPE_SetTargetConsistencyStatesResp);

   if (!respMsg)
   {
      setBuddyNeedsResyncEntry = timerQ.enqueue(std::chrono::seconds(5), [&, localNodeID] {
         retrySetBuddyNeedsResyncComm(mgmtNode, bgm, timerQ, localNodeID);
      });
      return false;
   }

   auto* respMsgCast = (SetTargetConsistencyStatesRespMsg*)respMsg.get();

   if (respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
   {
      LOG(GENERAL, CRITICAL, "Management node did not accept target states.", buddyTargetID,
            needsResync);
      buddyNeedsResyncFile->write(BUDDY_RESYNC_NOT_REQUIRED);
      return true;
   }

   buddyNeedsResyncFile->write(state & ~BUDDY_RESYNC_UNACKED_FLAG);
   if (state & BUDDY_RESYNC_REQUIRED_FLAG)
      LOG(GENERAL, CRITICAL, "Marked secondary buddy for needed resync.",
            ("primary node", localNodeID.val()));
   return true;
}

}

namespace BuddyCommTk
{

   void prepareBuddyNeedsResyncState(Node& mgmtNode, const MirrorBuddyGroupMapper& bgm,
         TimerQueue& timerQ, const NumNodeID localNodeID)
   {
      buddyNeedsResyncFile = std::make_unique<PreallocatedFile<uint8_t>>(
            BUDDY_NEEDS_RESYNC_FILENAME, S_IRUSR | S_IWUSR);

      if (buddyNeedsResyncFile->read().get_value_or(0) & BUDDY_RESYNC_UNACKED_FLAG)
      {
         setBuddyNeedsResyncEntry = timerQ.enqueue(std::chrono::seconds(0), [&, localNodeID] {
            retrySetBuddyNeedsResyncComm(mgmtNode, bgm, timerQ, localNodeID);
         });
      }
   }

   void checkBuddyNeedsResync()
   {
      App* app = Program::getApp();
      MirrorBuddyGroupMapper* metaBuddyGroups = app->getMetaBuddyGroupMapper();
      TargetStateStore* metaNodeStates = app->getMetaStateStore();
      InternodeSyncer* internodeSyncer = app->getInternodeSyncer();
      BuddyResyncer* buddyResyncer = app->getBuddyResyncer();

      NumNodeID localID = app->getLocalNodeNumID();
      bool isPrimary;
      NumNodeID buddyID = NumNodeID(metaBuddyGroups->getBuddyTargetID(localID.val(), &isPrimary) );

      if (isPrimary) // Only do the check if we are the primary.
      {
         // check if the secondary is set to needs-resync by the mgmtd.
         TargetConsistencyState consistencyState = internodeSyncer->getNodeConsistencyState();

         // If our own state is not good, don't start resync (wait until InternodeSyncer sets us
         // good again).
         if (consistencyState != TargetConsistencyState_GOOD)
         {
            LOG_DEBUG(__func__, Log_DEBUG,
               "Local node state is not good, won't check buddy state.");
            return;
         }

         CombinedTargetState buddyState;
         if (!metaNodeStates->getState(buddyID.val(), buddyState) )
         {
            LOG_DEBUG(__func__, Log_DEBUG, "Buddy state is invalid for node ID "
               + buddyID.str() + ".");
            return;
         }

         if (buddyState == CombinedTargetState(TargetReachabilityState_ONLINE,
             TargetConsistencyState_NEEDS_RESYNC) )
         {
            FhgfsOpsErr resyncRes = buddyResyncer->startResync();

            if (resyncRes == FhgfsOpsErr_SUCCESS)
            {
               LOG(MIRRORING, WARNING,
                     "Starting buddy resync job.", ("Buddy node ID", buddyID.val()));
            }
            else if (resyncRes == FhgfsOpsErr_INUSE)
            {
               LOG(MIRRORING, WARNING,
                     "Resync job currently running.", ("Buddy node ID", buddyID.val()));
            }
            else
            {
               LOG(MIRRORING, WARNING,
                     "Starting buddy resync job failed.", ("Buddy node ID", buddyID.val()));
            }
         }
      }
   }

   void setBuddyNeedsResync(const std::string& path, bool needsResync)
   {
      auto* const app = Program::getApp();

      ::setBuddyNeedsResync(*app->getMgmtNodes()->referenceFirstNode(),
            *app->getMetaBuddyGroupMapper(), *app->getTimerQueue(),
            app->getLocalNode().getNumID(), needsResync);
   }

   bool getBuddyNeedsResync()
   {
      return ::getBuddyNeedsResync();
   }
};

