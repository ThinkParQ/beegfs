#ifndef STORAGE_NODE_OP_STATS_H_
#define STORAGE_NODE_OP_STATS_H_


#include <common/nodes/OpCounter.h>
#include <common/nodes/OpCounter.h>
#include <common/nodes/Node.h>
#include <common/nodes/NodeOpStats.h>

/**
 * Per-user storage server operation statistics.
 */
struct UserStorageOpStats
{
   AtomicUInt64 numOps; // number of operations done by this user (incl read and write)
   AtomicUInt64 numBytesWritten; // number of bytes written by this user
   AtomicUInt64 numBytesRead; // number of bytes read by this user
};


/**
 * Count storage filesystem operations
 */
class StorageNodeOpStats : public NodeOpStats
{
   public:

      /**
       * Update operation counter for the given nodeIP and userID.
       *
       * @param node IP of the node
       * @param operation the filesystem operation to count
       *
       * NOTE: If you need to change something here, don't forget to update the similar method
       *       below.
       */
      void updateNodeOp(unsigned nodeIP, StorageOpCounterTypes opType, unsigned userID)
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
                  NodeOpCounterMapVal(nodeIP, StorageOpCounter() ) ).first;
            }

            if(userIter == userCounterMap.end() )
            {
               userIter = userCounterMap.insert(
                  NodeOpCounterMapVal(userID, StorageOpCounter() ) ).first;
            }
         }

         nodeIter->second.increaseOpCounter(opType);
         userIter->second.increaseOpCounter(opType);

         safeLock.unlock();
      }

      /**
       * Update operation counter for the given nodeIP and userID.
       * Almost similar as above, it just takes the additional bytes parameter and calls
       * increaseOpBytes().
       *
       * NOTE: If you need to change something here, don't forget to update the similar method
       *       above.
       *
       * @param opType only StorageOpCounter_WRITEOPS and _READOPS allowed as value, they will be
       * internally converted to the corresponding PerUserStorageOpCounter types.
       */
      void updateNodeOp(unsigned nodeIP, StorageOpCounterTypes operation, uint64_t bytes,
         unsigned userID)
      {
         SafeRWLock safeLock(&lock, SafeRWLock_READ);

         NodeOpCounterMapIter nodeIter = clientCounterMap.find(nodeIP);
         NodeOpCounterMapIter userIter = userCounterMap.find(userID);

         if( (nodeIter == clientCounterMap.end() ) ||
             (userIter == userCounterMap.end() ) )
         { // nodeID or userID NOT found in map yet, we need a write lock

            safeLock.unlock(); // possible race, but insert below is a NOOP if key already exists
            safeLock.lock(SafeRWLock_WRITE);

            if(nodeIter == clientCounterMap.end() )
            {
               nodeIter = clientCounterMap.insert(
                  NodeOpCounterMapVal(nodeIP, StorageOpCounter() ) ).first;
            }

            if(userIter == userCounterMap.end() )
            {
               userIter = userCounterMap.insert(
                  NodeOpCounterMapVal(userID, StorageOpCounter() ) ).first;
            }
         }

         nodeIter->second.increaseStorageOpBytes(operation, bytes);
         userIter->second.increaseStorageOpBytes(operation, bytes);

         safeLock.unlock();
      }

};

#endif /* STORAGE_NODE_OP_STATS_H_ */
