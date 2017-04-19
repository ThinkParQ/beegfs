#ifndef PERSONALWORKQUEUE_H_
#define PERSONALWORKQUEUE_H_

#include <common/components/worker/Work.h>
#include <common/toolkit/NamedException.h>
#include <common/Common.h>


DECLARE_NAMEDEXCEPTION(PersonalWorkQueueException, "PersonalWorkQueueException")


/*
 * Personal queues are intended to assign work directly to a specific worker thread.
 *
 * This is useful when we need to make sure that each worker gets a certain work request at least
 * once, e.g. to synchronize workers for fsck modification logging.
 *
 * This class has no own mutex for thread-safety, it is sync'ed via the MultiWorkQueue mutex.
 * So adding work to it is done via MultiWorkQueue methods.
 *
 * Note: Workers always prefer requests in the personal queue over requests in the other queues,
 * so keep possible starvation of requests in other queues in mind when you use personal queues.
 */
class PersonalWorkQueue
{
   friend class MultiWorkQueue; /* to make sure that our methods are not called without the
                                   MultiWorkQueue mutex being held. */

   public:
      PersonalWorkQueue() {}

      ~PersonalWorkQueue()
      {
         // delete remaining work packets
         for(WorkListIter iter = workList.begin(); iter != workList.end(); iter++)
            delete(*iter);
      }


   private:
      WorkList workList;


   private:
      // inliners

      void addWork(Work* work)
      {
         workList.push_back(work);
      }

      /*
       * get the next work package (and remove it from the queue).
       * caller must ensure that the queue is not empty before calling this.
       */
      Work* getAndPopFirstWork()
      {
         if(unlikely(workList.empty() ) ) // should never happen
            throw PersonalWorkQueueException("Work requested from empty queue");

         Work* work = *workList.begin();
         workList.pop_front();

         return work;
      }

      bool getIsWorkListEmpty()
      {
         return workList.empty();
      }

      size_t getWorkListSize()
      {
         return workList.size();
      }

};

#endif /*PERSONALWORKQUEUE_H_*/
