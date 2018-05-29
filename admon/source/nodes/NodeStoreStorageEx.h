#ifndef NODESTORESTORAGEEX_H_
#define NODESTORESTORAGEEX_H_

/*
 * NodeStoreStorageEx extends NodeStore from beegfs_common to support StorageNodeEx objects
 *
 * The node stores are derived in admon for each of the different node classes to handle their
 * special data structures in the addNode and addOrUpdateNode methods
 */

#include <common/nodes/NodeStore.h>
#include <nodes/StorageNodeEx.h>


class NodeStoreStorageEx : public NodeStoreServers
{
   public:
      NodeStoreStorageEx();

     bool addOrUpdateNode(std::shared_ptr<StorageNodeEx> node);
     virtual bool addOrUpdateNode(std::shared_ptr<Node> node);
     virtual bool addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID);

     bool statusStorage(NumNodeID nodeID, std::string *outInfo);

     unsigned sessionCountStorage(NumNodeID nodeID);

     bool getGeneralStorageNodeInfo(NumNodeID nodeID, GeneralNodeInfo *outInfo);
     NicAddressList getStorageNodeNicList(NumNodeID nodeID);
     void storageTargetsInfo(NumNodeID nodeID, StorageTargetInfoList *outStorageTargets);

     std::string timeSinceLastMessageStorage(NumNodeID nodeID);

     std::string diskSpaceTotalSum(std::string *outUnit);
     std::string diskSpaceFreeSum(std::string *outUnit);
     std::string diskSpaceUsedSum(std::string *outUnit);
     std::string diskRead(NumNodeID nodeID, uint timespanM, std::string *outUnit);
     std::string diskWrite(NumNodeID nodeID, uint timespanM, std::string *outUnit);
     std::string diskReadSum(uint timespanM, std::string *outUnit);
     std::string diskWriteSum(uint timespanM, std::string *outUnit);
     void diskPerfRead(NumNodeID nodeID, uint timespanM, UInt64List *outListTime,
        UInt64List *outListReadPerSec, UInt64List *outListAverageTime,
        UInt64List *outListAverageReadPerSec, uint64_t startTimestampMS, uint64_t endTimestampMS);
     void diskPerfWrite(NumNodeID nodeID, uint timespanM, UInt64List *outListTime,
        UInt64List *outListWritePerSec, UInt64List *outListAverageTime,
        UInt64List *outListAverageWritePerSec, uint64_t startTimestampMS, uint64_t endTimestampMS);
     void diskPerfReadSum(uint timespanM, UInt64List *outListTime, UInt64List *outListReadPerSec,
        UInt64List *outListAverageTime, UInt64List *outListAverageReadPerSec,
        uint64_t startTimestampMS, uint64_t endTimestampMS);
     void diskPerfWriteSum(uint timespanM, UInt64List *outListTime, UInt64List *outListWritePerSec,
        UInt64List *outListAverageTime, UInt64List *outListAverageWritePerSec,
        uint64_t startTimestampMS, uint64_t endTimestampMS);
};

#endif /*NODESTORESTORAGEEX_H_*/
