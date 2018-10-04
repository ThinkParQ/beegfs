#ifndef META_NODE_OP_STATS_H_
#define META_NODE_OP_STATS_H_

#include <common/nodes/OpCounter.h>
#include <common/threading/SafeRWLock.h>
#include <common/nodes/Node.h>
#include <common/nodes/NodeOpStats.h>

/**
 * Count filesystem metadata operations
 */
class MetaNodeOpStats : public NodeOpStats
{
   public:

      /**
       * Update operation counter for the given nodeIP and userID.
       *
       * @param nodeIP IP of the node
       * @param opType the filesystem operation to count
       */
      void updateNodeOp(unsigned nodeIP, MetaOpCounterTypes opType, unsigned userID)
      {
         SafeRWLock safeLock(&lock, SafeRWLock_READ);

         NodeOpCounterMapIter nodeIter = clientCounterMap.find(nodeIP);
         NodeOpCounterMapIter userIter = userCounterMap.find(userID);

         if( (nodeIter == clientCounterMap.end() ) ||
             (userIter == userCounterMap.end() ) )
         { // nodeIP or userID NOT found in map yet, we need a write lock

            safeLock.unlock(); // possible race, but insert below is a NOOP if key already exists
            safeLock.lock(SafeRWLock_WRITE);

            if(nodeIter == clientCounterMap.end() )
            {
               nodeIter = clientCounterMap.insert(
                  NodeOpCounterMapVal(nodeIP, MetaOpCounter() ) ).first;
            }

            if(userIter == userCounterMap.end() )
            {
               userIter = userCounterMap.insert(
                  NodeOpCounterMapVal(userID, MetaOpCounter() ) ).first;
            }
         }

         nodeIter->second.increaseOpCounter(opType);
         userIter->second.increaseOpCounter(opType);

         safeLock.unlock();
      }
};

#endif /* META_NODE_OP_STATS_H_ */

