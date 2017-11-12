#ifndef LOCKENTRYNOTIFICATIONWORK_H_
#define LOCKENTRYNOTIFICATIONWORK_H_

#include <common/Common.h>
#include <common/components/worker/Work.h>
#include <common/components/AbstractDatagramListener.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/SafeMutexLock.h>
#include <storage/Locking.h>


typedef std::list<EntryLockDetails> LockEntryNotifyList;
typedef LockEntryNotifyList::iterator LockEntryNotifyListIter;
typedef LockEntryNotifyList::const_iterator LockEntryNotifyListCIter;


class LockEntryNotificationWork : public Work
{
   public:
      /**
       * @param notifyList will be owned and freed by this object, so do not use or free it after
       * calling this.
       */
      LockEntryNotificationWork(std::string parentEntryID, std::string entryID,
         LockEntryNotifyList* notifyList) :
         parentEntryID(parentEntryID), entryID(entryID), notifyList(notifyList)
      {
         /* all assignments done in initializer list */
      }

      virtual ~LockEntryNotificationWork()
      {
         delete notifyList;
      }


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      // static attributes & methods

      static Mutex ackCounterMutex;
      static unsigned ackCounter;

      static unsigned incAckCounter();

      // instance attributes & methods

      std::string parentEntryID;
      std::string entryID;
      LockEntryNotifyList* notifyList;

      Mutex* getDGramLisMutex(AbstractDatagramListener* dgramLis);
};


#endif /* LOCKENTRYNOTIFICATIONWORK_H_ */
