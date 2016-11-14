#ifndef CAPACITYINFO_H_
#define CAPACITYINFO_H_

#include <string>

#include <common/nodes/CapacityPoolType.h>
#include <common/nodes/NumNodeID.h>

/**
  * This class stores information intended to be used during node->pool assignment.
  * Stores the following information about a target:
  * - NodeIDWithTypeStr - node ID as a string (for logging)
  * - free space on a target
  * - pool assignment
  */
class CapacityInfo
{
   public:
      /**
       * Constructor.
       * Target pool is initialized to NORMAL, because updateTargetPool can only assign
       * lower ranked target pools (higher index in enum).
       */
      CapacityInfo(NumNodeID nodeID) :
         nodeID(nodeID), freeSpace(-1), pool(CapacityPool_NORMAL)
      { }

      /**
       * Constructor to initialize all the members
       */
      CapacityInfo(NumNodeID nodeID, const std::string& nodeIDWithTypeStr,
            uint64_t freeSpace, uint64_t freeInodes, CapacityPoolType pool) :
         nodeID(nodeID), nodeIDWithTypeStr(nodeIDWithTypeStr), freeSpace(freeSpace),
         freeInodes(freeInodes), pool(pool)
      { }


   private:
      const NumNodeID nodeID;
      std::string nodeIDWithTypeStr;
      int64_t freeSpace;
      int64_t freeInodes;
      CapacityPoolType pool;


   public:
      // accessors
      NumNodeID getNodeID() const
      {
         return this->nodeID;
      }

      std::string const& getNodeIDWithTypeStr() const
      {
         return this->nodeIDWithTypeStr;
      }

      int64_t getFreeSpace() const
      {
         return this->freeSpace;
      }

      int64_t getFreeInodes() const
      {
         return this->freeInodes;
      }

      CapacityPoolType getTargetPool() const
      {
         return this->pool;
      }

      void setNodeIDWithTypeStr(const std::string& nodeIDWithTypeStr)
      {
         this->nodeIDWithTypeStr = nodeIDWithTypeStr;
      }

      void setFreeSpace(const int64_t freeSpace)
      {
         this->freeSpace = freeSpace;
      }

      void setFreeInodes(const int64_t freeInodes)
      {
         this->freeInodes = freeInodes;
      }

      void setTargetPoolType(const CapacityPoolType targetPool)
      {
         this->pool = targetPool;
      }

      /**
       * Assigns the pool type of the target only if the new pool is "worse" (less free space)
       * than the current pool.
       */
      void updateTargetPoolType(const CapacityPoolType targetPool)
      {
         this->pool = BEEGFS_MAX(targetPool, this->pool);
      }
};

typedef std::list<CapacityInfo> CapacityInfoList;
typedef CapacityInfoList::iterator CapacityInfoListIter;
typedef CapacityInfoList::const_iterator CapacityInfoListConstIter;


/**
 * extends the CapacityInfo class so that it's able to store a TargetID (needed for
 * target->pool assignment of storage targets.
 */
class TargetCapacityInfo : public CapacityInfo
{
   public:
      TargetCapacityInfo(const uint16_t targetID, const NumNodeID nodeID) :
            CapacityInfo(nodeID), targetID(targetID)
      { }

   private:
      const uint16_t targetID;

   public:
      uint16_t getTargetID() const
      {
         return this->targetID;
      }
};

typedef std::list<TargetCapacityInfo> TargetCapacityInfoList;
typedef TargetCapacityInfoList::iterator TargetCapacityInfoListIter;
typedef TargetCapacityInfoList::const_iterator TargetCapacityInfoListConstIter;


#endif /* CAPACITYINFO_H_ */
