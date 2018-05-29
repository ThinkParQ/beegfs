#include <components/worker/LockEntryNotificationWork.h>
#include <components/worker/LockRangeNotificationWork.h>
#include <program/Program.h>
#include "LockingNotifier.h"


/**
 * @param notifyList will be owned and freed by the LockingNotifier, so do not use or free it after
 * calling this.
 */
void LockingNotifier::notifyWaitersEntryLock(LockEntryNotifyType lockType,
   const std::string& parentEntryID, const std::string& entryID, bool isBuddyMirrored,
   LockEntryNotifyList notifyList)
{
   // just delegate to comm slaves

   MultiWorkQueue* slaveQ = Program::getApp()->getCommSlaveQueue();

   Work* work = new LockEntryNotificationWork(lockType, parentEntryID, entryID, isBuddyMirrored,
         std::move(notifyList));

   slaveQ->addDirectWork(work);
}

/**
 * @param notifyList will be owned and freed by the LockingNotifier, so do not use or free it after
 * calling this.
 */
void LockingNotifier::notifyWaitersRangeLock(const std::string& parentEntryID,
   const std::string& entryID, bool isBuddyMirrored, LockRangeNotifyList notifyList)
{
   // just delegate to comm slaves

   MultiWorkQueue* slaveQ = Program::getApp()->getCommSlaveQueue();

   Work* work = new LockRangeNotificationWork(parentEntryID, entryID, isBuddyMirrored,
         std::move(notifyList));

   slaveQ->addDirectWork(work);
}
