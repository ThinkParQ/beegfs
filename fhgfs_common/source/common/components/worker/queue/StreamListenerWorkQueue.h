#ifndef COMMON_COMPONENTS_WORKER_QUEUE_STREAMLISTENERWORKQUEUE_H_
#define COMMON_COMPONENTS_WORKER_QUEUE_STREAMLISTENERWORKQUEUE_H_


#include <common/components/worker/Work.h>



#define STREAMLISTENERWORKQUEUE_DEFAULT_USERID  (~0) // usually similar to NETMESSAGE_DEFAULT_USERID


class StreamListenerWorkQueue
{
   public:
      virtual ~StreamListenerWorkQueue() {};

      virtual void addDirectWork(Work* work,
         unsigned userID = STREAMLISTENERWORKQUEUE_DEFAULT_USERID) = 0;
      virtual void addIndirectWork(Work* work,
         unsigned userID = STREAMLISTENERWORKQUEUE_DEFAULT_USERID) = 0;
};

#endif /* COMMON_COMPONENTS_WORKER_QUEUE_STREAMLISTENERWORKQUEUE_H_ */
