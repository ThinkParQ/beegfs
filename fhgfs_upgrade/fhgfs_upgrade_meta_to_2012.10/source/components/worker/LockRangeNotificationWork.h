#ifndef LOCKRANGENOTIFICATIONWORK_H_
#define LOCKRANGENOTIFICATIONWORK_H_


#include <common/components/worker/Work.h>
#include <common/components/AbstractDatagramListener.h>
#include <common/threading/SafeMutexLock.h>
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
      LockRangeNotificationWork(std::string parentEntryID, std::string entryID,
         LockRangeNotifyList* notifyList) : parentEntryID(parentEntryID), entryID(entryID),
         notifyList(notifyList)
      {
         /* all assignments done in initializer list */
      }

      virtual ~LockRangeNotificationWork()
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
      LockRangeNotifyList* notifyList;

      Mutex* getDGramLisMutex(AbstractDatagramListener* dgramLis);
};


#endif /* LOCKRANGENOTIFICATIONWORK_H_ */
