#ifndef LOCKENTRYNOTIFICATIONWORK_H_
#define LOCKENTRYNOTIFICATIONWORK_H_

#include <common/Common.h>
#include <common/components/worker/Work.h>
#include <common/components/AbstractDatagramListener.h>
#include <common/storage/StorageErrors.h>
#include <storage/Locking.h>


class FileInode; // forward declaration


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
      LockEntryNotificationWork(LockEntryNotifyType lockType, const std::string& parentEntryID,
            const std::string& entryID, bool isBuddyMirrored, LockEntryNotifyList notifyList) :
         lockType(lockType), parentEntryID(parentEntryID), entryID(entryID),
         isBuddyMirrored(isBuddyMirrored), notifyList(std::move(notifyList))
      {
      }

      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      // static attributes & methods

      static Mutex ackCounterMutex;
      static unsigned ackCounter;

      static unsigned incAckCounter();

      // instance attributes & methods

      LockEntryNotifyType lockType;
      std::string parentEntryID;
      std::string entryID;
      bool isBuddyMirrored;
      LockEntryNotifyList notifyList;

      void unlockWaiter(FileInode& inode, EntryLockDetails* lockDetails);
      void cancelAllWaiters(FileInode& inode);

      Mutex* getDGramLisMutex(AbstractDatagramListener* dgramLis);
};


#endif /* LOCKENTRYNOTIFICATIONWORK_H_ */
