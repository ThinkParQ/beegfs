#ifndef LOCALNODECONNPOOL_H_
#define LOCALNODECONNPOOL_H_

#include <common/components/worker/LocalConnWorker.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/nodes/NodeConnPool.h>
#include <common/Common.h>


typedef std::list<LocalConnWorker*> LocalConnWorkerList;
typedef LocalConnWorkerList::iterator LocalConnWorkerListIter;

/**
 * This class is thread-safe
 */
class LocalNodeConnPool : public NodeConnPool
{
   public:
      LocalNodeConnPool(NicAddressList& nicList);
      virtual ~LocalNodeConnPool();
      
      Socket* acquireStreamSocketEx(bool allowWaiting);
      void releaseStreamSocket(Socket* sock);
      void invalidateStreamSocket(Socket* sock);
      
   private:
      NicAddressList nicList;
      LocalConnWorkerList connWorkerList;
      
      unsigned availableConns; // available established conns
      unsigned establishedConns; // not equal to connList.size!!
      unsigned maxConns;
      
      int numCreatedWorkers;
      
      Mutex mutex;
      Condition changeCond;
      
   public:
      // getters & setters
      
      NicAddressList getNicList()
      {
         // Note: Not synced (like in normal NodeConnPool) because the nicList will never
         //    change in the local connpool impl.
         
         return nicList;
      }


};

#endif /*LOCALNODECONNPOOL_H_*/
