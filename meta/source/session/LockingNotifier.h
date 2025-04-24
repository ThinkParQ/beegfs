#pragma once

#include <common/Common.h>
#include <storage/Locking.h>
#include <components/worker/LockEntryNotificationWork.h>
#include <components/worker/LockRangeNotificationWork.h>


/**
 * Creates work packages to notify waiting clients/processes about file lock grants.
 */
class LockingNotifier
{
   public:
      static void notifyWaitersEntryLock(LockEntryNotifyType lockType,
         const std::string& parentEntryID, const std::string& entryID, bool isBuddyMirrored,
         LockEntryNotifyList notifyList);
      static void notifyWaitersRangeLock(const std::string& parentEntryID,
         const std::string& entryID, bool isBuddyMirrored, LockRangeNotifyList notifyList);


   private:
      LockingNotifier() {}

};

