/*
 * Count meta filesystem operations
 *
 */
#ifndef META_NODE_OP_STATS_H_
#define META_NODE_OP_STATS_H_

#include <common/nodes/OpCounter.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/SafeRWLock.h>
#include <common/nodes/Node.h>
#include <common/nodes/NodeOpStats.h>

#ifdef FHGFS_DEBUG
   #define METAOPSTATS_SIMULATED_DEBUG_CLIENTS 10000
#endif

class MetaNodeOpStats : public NodeOpStats
{
   public:

      MetaNodeOpStats()
      {
         #ifdef FHGFS_DEBUG
            // simulate lots of clients here, to test if their stats appear in fhgfs-ctl
            for (int ia=1; ia<METAOPSTATS_SIMULATED_DEBUG_CLIENTS; ia++)
               this->updateNodeOp(ia, MetaOpCounter_ACK);
         #endif
      }

      /**
       * Update the Op counter for the given node
       *
       * @param node       - IP of the node
       * @param operation  - The filesystem operation to count
       */
      void updateNodeOp(uint64_t node, enum MetaOpCounterTypes operation)
      {
         SafeRWLock safeLock(&lock, SafeRWLock_READ);
         if (nodeCounterMap.find(node) == nodeCounterMap.end() )
         { // node NOT found in map yet, we need a write lock
            safeLock.unlock(); // possible race, but insert is a NOOP if key already exists
            safeLock.lock(SafeRWLock_WRITE);
            OpCounter opCounter(NODETYPE_Meta);
            nodeCounterMap.insert(clientNodeOpCounterMap::value_type(node, opCounter));
         }

         ClientNodeOpCounterMapIter iter = nodeCounterMap.find(node);
         iter->second.increaseOpCounter((int)operation);

         safeLock.unlock();
      }
};

#endif /* META_NODE_OP_STATS_H_ */

