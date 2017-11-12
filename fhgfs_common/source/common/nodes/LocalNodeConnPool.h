#ifndef LOCALNODECONNPOOL_H_
#define LOCALNODECONNPOOL_H_

#include <common/components/worker/UnixConnWorker.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/nodes/NodeConnPool.h>
#include <common/Common.h>


typedef std::list<UnixConnWorker*> UnixConnWorkerList;
typedef UnixConnWorkerList::iterator UnixConnWorkerListIter;

/**
 * This is the conn pool which is used when a node sends network messages to itself, e.g. like a
 * single mds would do in case of an incoming mkdir msg.
 * It is based on unix sockets for inter-process communication instead of network sockets and
 * creates a handler thread for each established connection to process "incoming" msgs.
 */
class LocalNodeConnPool : public NodeConnPool
{
   public:
      LocalNodeConnPool(Node& parentNode, NicAddressList& nicList);
      LocalNodeConnPool(Node& parentNode, std::string namedSocketPath);
      virtual ~LocalNodeConnPool();

      Socket* acquireStreamSocketEx(bool allowWaiting);
      void releaseStreamSocket(Socket* sock);
      void invalidateStreamSocket(Socket* sock);

   private:
      NicAddressList nicList;
      UnixConnWorkerList connWorkerList;

      unsigned availableConns; // available established conns
      unsigned establishedConns; // not equal to connList.size!!
      unsigned maxConns;

      std::string namedSocketPath;

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
