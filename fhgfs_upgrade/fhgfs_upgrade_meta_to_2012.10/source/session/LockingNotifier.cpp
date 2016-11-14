#include <components/worker/LockEntryNotificationWork.h>
#include <components/worker/LockRangeNotificationWork.h>
#include <program/Program.h>
#include "LockingNotifier.h"


/**
 * @param notifyList will be owned and freed by the LockingNotifier, so do not use or free it after
 * calling this.
 */
void LockingNotifier::notifyWaiters(std::string parentEntryID, std::string entryID,
   LockEntryNotifyList* notifyList)
{
   // just delegate to comm slaves

   MultiWorkQueue* slaveQ = Program::getApp()->getCommSlaveQueue();

   Work* work = new LockEntryNotificationWork(parentEntryID, entryID, notifyList);

   slaveQ->addDirectWork(work);
}

/**
 * @param notifyList will be owned and freed by the LockingNotifier, so do not use or free it after
 * calling this.
 */
void LockingNotifier::notifyWaiters(std::string parentEntryID, std::string entryID,
   LockRangeNotifyList* notifyList)
{
   // just delegate to comm slaves

   MultiWorkQueue* slaveQ = Program::getApp()->getCommSlaveQueue();

   Work* work = new LockRangeNotificationWork(parentEntryID, entryID, notifyList);

   slaveQ->addDirectWork(work);
}
