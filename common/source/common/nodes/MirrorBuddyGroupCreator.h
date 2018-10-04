#ifndef COMMON_NODES_MIRRORBUDDYGROUPCREATOR_H_
#define COMMON_NODES_MIRRORBUDDYGROUPCREATOR_H_

#include <common/Common.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/NodeStore.h>
#include <common/nodes/RootInfo.h>
#include <common/nodes/StoragePoolStore.h>



// key: ServerNumID, value: counter of primary targets
typedef std::map<NumNodeID, size_t> PrimaryTargetCounterMap;
typedef PrimaryTargetCounterMap::iterator PrimaryTargetCounterMapIter;
typedef PrimaryTargetCounterMap::const_iterator PrimaryTargetCounterMapCIter;
typedef PrimaryTargetCounterMap::value_type PrimaryTargetCounterMapVal;


class MirrorBuddyGroupCreator
{
   public:
      MirrorBuddyGroupCreator(NodeStore* mgmtNodes, NodeStore* metaNodes, const RootInfo* metaRoot,
         StoragePoolStore* storagePoolStore)
         : cfgForce(false),
           cfgDryrun(false),
           cfgUnique(false),
           nodeType(NODETYPE_Invalid),
           mgmtNodes(mgmtNodes),
           metaNodes(metaNodes),
           metaRoot(metaRoot),
           storagePoolStore(storagePoolStore)
      {}

      FhgfsOpsErr addGroup(uint16_t primaryTargetID, uint16_t secondaryTargetID,
         uint16_t forcedGroupID);

      static uint16_t generateID(const UInt16List* usedMirrorBuddyGroups);

   protected:
      bool cfgForce; // ignore warnings/errors during automatic mode
      bool cfgDryrun; // only print the selected configuration
      bool cfgUnique; // use unique MirrorBuddyGroupIDs. The ID is not used as storage targetNumID

      NodeType nodeType; // the node type (meta / storage)
      const NodeStore* mgmtNodes; // NodeStore holding the management node
      const NodeStore* metaNodes;
      const RootInfo* metaRoot;
      const StoragePoolStore* storagePoolStore;

      // Note: Derived class is responsible for filling the nodeStore and the targetMappers.
      NodeStore* nodes; // NodeStore holding the storage / meta nodes depending on nodeType
      TargetMapper systemTargetMapper; // TargetMapper with all storage targets of the system, in
                                       // case nodeType==NODETYPE_Meta, only contains nodeIDs.
      TargetMapper localTargetMapper; // Contains list of targets and the nodes they belong to in
                                      // case nodeType == NODETYPE_Storage.
                                      // For nodeType == NODETYPE_Meta, the map is nodeID->nodeID.

      FhgfsOpsErr addGroupComm(uint16_t primaryID, uint16_t secondaryID,
         uint16_t forcedGroupID, uint16_t& outNewGroupID) const;

      bool removeTargetsFromExistingMirrorBuddyGroups(const UInt16List* oldPrimaryIDs,
         const UInt16List* oldSecondaryIDs);
      uint16_t findNextTarget(NumNodeID nodeNumIdToIgnore,
         StoragePoolId storagePoolId = StoragePoolStore::INVALID_POOL_ID);
      bool checkSizeOfTargets() const;
      FhgfsOpsErr generateMirrorBuddyGroups(UInt16List* outBuddyGroupIDs,
         MirrorBuddyGroupList* outBuddyGroups, const UInt16List* usedMirrorBuddyGroupIDs);
      void selectPrimaryTarget(PrimaryTargetCounterMap* primaryUsed,
         uint16_t* inOutPrimaryID, uint16_t* inOutSecondaryID) const;
      bool createMirrorBuddyGroups(FhgfsOpsErr retValGeneration, const UInt16List* buddyGroupIDs,
         const MirrorBuddyGroupList* buddyGroups);

      uint16_t generateUniqueID(const TargetMapper* targetMapper,
                                const UInt16List* usedMirrorBuddyGroups) const;
};

#endif /* COMMON_NODES_MIRRORBUDDYGROUPCREATOR_H_ */
