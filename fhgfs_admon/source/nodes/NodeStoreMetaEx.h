#ifndef NODESTOREMETAEX_H_
#define NODESTOREMETAEX_H_

/*
 * NodeStoreMetaEx extends NodeStore from beegfs_common to support MetaNodeEx objects
 *
 * The node stores are derived in admon for each of the different node classes to handle their
 * special data structures in the addNode and addOrUpdateNode methods
 */

#include <common/nodes/NodeStore.h>
#include <nodes/MetaNodeEx.h>


class NodeStoreMetaEx : public NodeStoreServers
{
   public:
      NodeStoreMetaEx() throw(InvalidConfigException);

     bool addOrUpdateNode(std::shared_ptr<MetaNodeEx> node);
     virtual bool addOrUpdateNode(std::shared_ptr<Node> node);
     virtual bool addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID);

     std::string isRootMetaNode(NumNodeID nodeID);
     std::string getRootMetaNode();
     std::string getRootMetaNodeIDWithTypeStr();
     std::string getRootMetaNodeAsTypedNodeID();

     bool statusMeta(NumNodeID nodeID, std::string *outInfo);

     unsigned sessionCountMeta(NumNodeID nodeID);

     bool getGeneralMetaNodeInfo(NumNodeID nodeID, GeneralNodeInfo *outInfo);
     NicAddressList getMetaNodeNicList(NumNodeID nodeID);

     void metaDataRequests(NumNodeID nodeID, uint timespanM, StringList *outListTime,
        StringList *outListQueuedRequests, StringList *outListWorkRequests);
     void metaDataRequestsSum(uint timespanM, StringList *outListTime,
        StringList *outListQueuedRequests, StringList *outListWorkRequests);

     std::string timeSinceLastMessageMeta(NumNodeID nodeID);
};

#endif /*NODESTOREMETAEX_H_*/
