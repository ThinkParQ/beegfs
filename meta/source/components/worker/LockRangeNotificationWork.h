#ifndef LOCKRANGENOTIFICATIONWORK_H_
#define LOCKRANGENOTIFICATIONWORK_H_


#include <common/components/worker/Work.h>
#include <common/components/AbstractDatagramListener.h>
#include <common/Common.h>

typedef std::list<RangeLockDetails> LockRangeNotifyList;
typedef LockRangeNotifyList::iterator LockRangeNotifyListIter;
typedef LockRangeNotifyList::const_iterator LockRangeNotifyListCIter;


class LockRangeNotificationWork : public Work
{
   public:
      /**
       * @param notifyList will be owned and freed by this object, so do not use or free it after
       * calling this.
       */
      LockRangeNotificationWork(const std::string& parentEntryID, const std::string& entryID,
            bool isBuddyMirrored, LockRangeNotifyList notifyList):
         parentEntryID(parentEntryID), entryID(entryID), isBuddyMirrored(isBuddyMirrored),
         notifyList(std::move(notifyList))
      {
         /* all assignments done in initializer list */
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
      bool isBuddyMirrored;
      LockRangeNotifyList notifyList;

      Mutex* getDGramLisMutex(AbstractDatagramListener* dgramLis);
};


#endif /* LOCKRANGENOTIFICATIONWORK_H_ */
