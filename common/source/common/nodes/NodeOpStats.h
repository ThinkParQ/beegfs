#ifndef NODEOPSTATS_H_
#define NODEOPSTATS_H_

#include <common/nodes/OpCounter.h>
#include <common/threading/SafeRWLock.h>
#include <common/nodes/Node.h>

// layout version ofthe vector transfered to fhgfs-ctl
#define OPCOUNTER_VEC_LAYOUT_VERS 1 // only update, if the layout changes incompatibly!

/* Element positions within the op-counter vector (one IP and its op-counter)
   NOTE: Try to avoid to change this part of the vector, as it will introduce incompatibilities! */
enum PerIPVectorPos
{
   OPCOUNTERVECTOR_POS_IP = 0,             // The IP address, always MUST be the first element
   OPCOUNTERVECTOR_POS_SUM = 1,            // sum of all op counters
};

#define OPCOUNTERVECTOR_POS_FIRSTCOUNTER OPCOUNTERVECTOR_POS_SUM // sum is also the first counter

/* Just a safety buffer so that we definitely cannot exceed the max buffer length due to
   serialization overhead (this overhead is difficult to compute from here) */
#define NODEOP_STATS_NET_MESSAGE_SAFETY_LENGTH 1024 * 10

/*
 * Element positions in the ClientStats vector (several IPs and their op-counter in this vector)
 */
enum StatsMetaVecPos
{
   NODEOPS_POS_NUMOPS = 0, // number of operations per IP

   // 0 if all IPs and their Ops could be transferred in a single vector, 1 if there are more data
   NODE_OPS_POS_MORE_DATA,

   NODE_OPS_POS_LAYOUT_VERSION, // layout version of the vector to detect incompatibilieties

   NODE_OPS_POS_NUM_RESERVED1, // reserved for later usage
   NODE_OPS_POS_NUM_RESERVED2, // reserved for later usage
   NODE_OPS_POS_NUM_RESERVED3, // reserved for later usage
   NODE_OPS_POS_NUM_RESERVED4, // reserved for later usage
   NODE_OPS_POS_NUM_RESERVED5, // reserved for later usage

   NODE_OPS_POS_FIRSTDATAELEMENT  // address of the data element including the IP
};

/* Elements of the vector to be taken by other (meta) fields.
   We can use the enum value here, as the enum start with zero */
#define STATS_VEC_RESERVED_ELEMENTS \
   (NODE_OPS_POS_FIRSTDATAELEMENT + OPCOUNTERVECTOR_POS_FIRSTCOUNTER)


typedef std::map<unsigned, OpCounter> NodeOpCounterMap; // key: nodeIP/userID, val: op counters
typedef NodeOpCounterMap::value_type NodeOpCounterMapVal;
typedef NodeOpCounterMap::iterator NodeOpCounterMapIter;

/**
 * Contains operation counters for per-client and per-user statistics.
 *
 * This is the common basis of "MetaNodeOpStats" and "StorageNodeOpStats", which provide the method
 * updateNodeOp() to update corresponding metadata and storage operation counters.
 */
class NodeOpStats
{
   public:
      bool mapToUInt64Vec(uint64_t cookieIP, size_t bufLen, bool wantPerUserStats,
         UInt64Vector *outVec);

   private:
      int getMaxIPsPerVector(OpCounter *opCounter, size_t bufLen);
      bool reserveVector(OpCounter *opCounter, size_t numIPs, UInt64Vector *outVec);

   protected:
      NodeOpCounterMap clientCounterMap; // maps IPs to corresponding operation counters
      NodeOpCounterMap userCounterMap; // maps userIDs to corresponding operation counters

      RWLock lock;

   public:

      /**
       * Erase the given node from the map
       *
       * @param IP of a node
       */
      void removeClientFromMap(unsigned nodeIP)
      {
         SafeRWLock safeLock(&lock, SafeRWLock_WRITE);
         clientCounterMap.erase(nodeIP); // erase by key
         safeLock.unlock();
      }

};


#endif /* NODEOPSTATS_H_ */
