#include <common/net/message/nodes/GetMirrorBuddyGroupsMsg.h>
#include <common/net/message/nodes/GetMirrorBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetNodesMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/nodes/GetTargetStatesMsg.h>
#include <common/net/message/nodes/GetTargetStatesRespMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/threading/PThread.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/SocketTk.h>
#include "NodesTk.h"

/**
 * Wait for the first node to appear in the management nodes store.
 *
 * @param currentThread the calling thread (used to catch self-terminate order, may be NULL)
 * @param hostname hostname of mgmtd service
 * @param portUDP udp port of mgmtd service
 * @param timeoutMS 0 for infinite
 * @return true if heartbeat received, false if cancelled because of termination order
 */
bool NodesTk::waitForMgmtHeartbeat(PThread* currentThread, AbstractDatagramListener* dgramLis,
   NodeStoreServers* mgmtNodes, std::string hostname, unsigned short portUDP, unsigned timeoutMS)
{
   bool gotMgmtHeartbeat = false;
   struct in_addr ipAddr;

   const int waitForNodeSleepMS = 750;
   Time startTime;
   Time lastRetryTime;
   unsigned nextRetryDelayMS = 0;

   HeartbeatRequestMsg msg;
   std::pair<char*, unsigned> msgBuf = MessagingTk::createMsgBuf(&msg);


   // wait for mgmt node to appear and periodically resend request
   while(!currentThread || !currentThread->getSelfTerminate() )
   {
      if(lastRetryTime.elapsedMS() >= nextRetryDelayMS)
      { // time to send request again
         bool getHostByNameRes = SocketTk::getHostByName(hostname.c_str(), &ipAddr);
         if(getHostByNameRes)
            dgramLis->sendto(msgBuf.first, msgBuf.second, 0, ipAddr, portUDP);

         lastRetryTime.setToNow();
         nextRetryDelayMS = getRetryDelayMS(startTime.elapsedMS() );
      }

      /* note: we can't sleep in waitForFirstNode() for too long, because we need to check
         getSelfTerminate() every now and then */

      gotMgmtHeartbeat = mgmtNodes->waitForFirstNode(waitForNodeSleepMS);
      if(gotMgmtHeartbeat)
         break; // got management node in store

      if(timeoutMS && (startTime.elapsedMS() >= timeoutMS) )
         break; // caller-given timeout expired
   }

   free(msgBuf.first);

   return gotMgmtHeartbeat;
}

/**
 * Downloads node list from given sourceNode.
 *
 * Note: If you intend to connect to these nodes, you probably want to call
 * NodesTk::applyLocalNicCapsToList() on the result list before connecting.
 *
 * @param sourceNode the node from which node you want to download
 * @param nodeType which type of node list you want to download
 * @param outNodeList caller is responsible for the deletion of the received nodes
 * @param outRootNumID numeric ID of root mds, may be NULL if caller is not interested
 * @param outRootIsBuddyMirrored is root directory on mds buddy mirrored, may be NULL if caller is
 * not interested
 * @return true if download successful
 */
bool NodesTk::downloadNodes(Node& sourceNode, NodeType nodeType, std::vector<NodeHandle>& outNodes,
   bool silenceLog, NumNodeID* outRootNumID, bool* outRootIsBuddyMirrored)
{
   GetNodesMsg msg(nodeType);
   RequestResponseArgs rrArgs(&sourceNode, &msg, NETMSGTYPE_GetNodesResp);

#ifndef BEEGFS_DEBUG
   if (silenceLog)
      rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                       | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif // BEEGFS_DEBUG

   // connect & communicate
   bool commRes = MessagingTk::requestResponse(&rrArgs);
   if(!commRes)
      return false;

   // handle result
   GetNodesRespMsg* respMsgCast = static_cast<GetNodesRespMsg*>(rrArgs.outRespMsg);

   respMsgCast->getNodeList().swap(outNodes);

   if(outRootNumID)
      *outRootNumID = respMsgCast->getRootNumID();

   if(outRootIsBuddyMirrored)
      *outRootIsBuddyMirrored = respMsgCast->getRootIsBuddyMirrored();

   return true;
}

/**
 * Downloads target mappings from given sourceNode.
 *
 * @param sourceNode     - the node to query about targets (usually management)
 * @param outTargetIDs   - list of targets sourceNode knows about
 * @param outNodeIDs     - nodeIDs corresponding to outTargetsIDs
 * @return true if download successful
 *
 * NOTE:  outTargetIDs and outNodeIDs match 1:1
 *        Example: targetA of nodeA is at list-position 10 of outTargetIDs, then 'nodeA' also is
 *                 at list position 10 of outNodeIDs. If a node has several targets that node
 *                 will appear several time in outNodeIDs, depending on its number of targets.
 */
bool NodesTk::downloadTargetMappings(Node& sourceNode, UInt16List* outTargetIDs,
   NumNodeIDList* outNodeIDs, bool silenceLog)
{
   GetTargetMappingsMsg msg;
   RequestResponseArgs rrArgs(&sourceNode, &msg, NETMSGTYPE_GetTargetMappingsResp);

#ifndef BEEGFS_DEBUG
   if (silenceLog)
      rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                       | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif // BEEGFS_DEBUG

   // connect & communicate
   bool commRes = MessagingTk::requestResponse(&rrArgs);
   if(!commRes)
      return false;

   // handle result
   GetTargetMappingsRespMsg* respMsgCast =
      static_cast<GetTargetMappingsRespMsg*>(rrArgs.outRespMsg);

   respMsgCast->getTargetIDs().swap(*outTargetIDs);
   respMsgCast->getNodeIDs().swap(*outNodeIDs);

   return true;
}

/*
 * note: outBuddyGroupIDs, outPrimaryTargetIDs and outSecondaryTargetIDs match 1:1
 *
 * @param nodeType the type of group to download.
 */
bool NodesTk::downloadMirrorBuddyGroups(Node& sourceNode, NodeType nodeType,
   UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs, UInt16List* outSecondaryTargetIDs,
   bool silenceLog)
{
   GetMirrorBuddyGroupsMsg msg(nodeType);
   RequestResponseArgs rrArgs(&sourceNode, &msg, NETMSGTYPE_GetMirrorBuddyGroupsResp);

#ifndef BEEGFS_DEBUG
   if (silenceLog)
      rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                       | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif // BEEGFS_DEBUG

   // connect & communicate
   bool commRes = MessagingTk::requestResponse(&rrArgs);
   if(!commRes)
      return false;

   // handle result
   GetMirrorBuddyGroupsRespMsg* respMsgCast =
      static_cast<GetMirrorBuddyGroupsRespMsg*>(rrArgs.outRespMsg);

   respMsgCast->getBuddyGroupIDs().swap(*outBuddyGroupIDs);
   respMsgCast->getPrimaryTargetIDs().swap(*outPrimaryTargetIDs);
   respMsgCast->getSecondaryTargetIDs().swap(*outSecondaryTargetIDs);

   return true;
}

/*
 * note: outTargetIDs, outReachabilityStates and outConsistencyStates match 1:1
 *
 * @param nodeType the type of states which should be downloaded (meta, storage).
 */
bool NodesTk::downloadTargetStates(Node& sourceNode, NodeType nodeType, UInt16List* outTargetIDs,
   UInt8List* outReachabilityStates, UInt8List* outConsistencyStates, bool silenceLog)
{
   GetTargetStatesMsg msg(nodeType);
   RequestResponseArgs rrArgs(&sourceNode, &msg, NETMSGTYPE_GetTargetStatesResp);

#ifndef BEEGFS_DEBUG
   if (silenceLog)
      rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                       | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif // BEEGFS_DEBUG

   bool commRes;

   // connect & communicate
   commRes = MessagingTk::requestResponse(&rrArgs);
   if(!commRes)
      return false;

   // handle result
   GetTargetStatesRespMsg* respMsgCast = static_cast<GetTargetStatesRespMsg*>(rrArgs.outRespMsg);

   if (outTargetIDs)
      respMsgCast->getTargetIDs().swap(*outTargetIDs);

   if (outReachabilityStates)
      respMsgCast->getReachabilityStates().swap(*outReachabilityStates);

   if (outConsistencyStates)
      respMsgCast->getConsistencyStates().swap(*outConsistencyStates);

   return true;
}

/**
 * Note: outBuddyGroupIDs, outPrimaryTargetIDs, and outSecondaryTargetIDs match 1:1;
 *       outTargetIDs and outTargetStates match 1:1.
 */
bool NodesTk::downloadStatesAndBuddyGroups(Node& sourceNode, NodeType nodeType,
   UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs, UInt16List* outSecondaryTargetIDs,
   UInt16List* outTargetIDs, UInt8List* outTargetReachabilityStates,
   UInt8List* outTargetConsistencyStates, bool silenceLog)
{
   GetStatesAndBuddyGroupsMsg msg(nodeType);
   RequestResponseArgs rrArgs(&sourceNode, &msg, NETMSGTYPE_GetStatesAndBuddyGroupsResp);

#ifndef BEEGFS_DEBUG
   if (silenceLog)
      rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                       | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif // BEEGFS_DEBUG

   bool commRes;

   // connect & communicate
   commRes = MessagingTk::requestResponse(&rrArgs);

   if (!commRes)
      return false;

   // handle result
   GetStatesAndBuddyGroupsRespMsg* respMsgCast =
      static_cast<GetStatesAndBuddyGroupsRespMsg*>(rrArgs.outRespMsg);

   respMsgCast->getBuddyGroupIDs().swap(*outBuddyGroupIDs);
   respMsgCast->getPrimaryTargetIDs().swap(*outPrimaryTargetIDs);
   respMsgCast->getSecondaryTargetIDs().swap(*outSecondaryTargetIDs);

   respMsgCast->getTargetIDs().swap(*outTargetIDs);
   respMsgCast->getReachabilityStates().swap(*outTargetReachabilityStates);
   respMsgCast->getConsistencyStates().swap(*outTargetConsistencyStates);

   return true;
}

/**
 * Node: This is probably only useful in very rare cases, because normally you would want to
 *    use NodeStore::syncNodes() instead of this add-only method.
 *
 * @param nodeList All the nodes will belong to the store afterwards, so don't use them anymore
 * after calling this method.
 */
void NodesTk::moveNodesFromListToStore(std::vector<NodeHandle>& nodes, AbstractNodeStore* nodeStore)
{
   for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
      nodeStore->addOrUpdateNode(*iter);

   nodes.clear();
}

/**
 * Note: Useful if you got the list e.g. via downloadNodes(), because that doesn't set the
 * localNicCaps.
 * Note: Make sure you didn't use the connPools of the nodes before calling this.
 */
void NodesTk::applyLocalNicCapsToList(Node& localNode, const std::vector<NodeHandle>& nodes)
{
   for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
   {
      Node& currentNode = **iter;

      // set local nic capabilities
      NicAddressList localNicList(localNode.getNicList() );
      NicListCapabilities localNicCaps;

      NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
      currentNode.getConnPool()->setLocalNicCaps(&localNicCaps);
   }
}

/**
 * Returns the timeout to wait before the next (communication) retry based on the time that the
 * caller already spent waiting for a reply.
 * This provides a convenient way to get increasing timeouts between retries.
 *
 * @param elapsedMS the time that the caller already waited for a reply, including all previous
 * retries.
 * @return number of milliseconds to wait before the next retry.
 */
unsigned NodesTk::getRetryDelayMS(int elapsedMS)
{
   if(elapsedMS < 1000*3)
   {
      return 1000*1 + Random().getNextInRange(-100, 100);
   }
   else
   if(elapsedMS < 1000*10)
   {
      return 1000*2;
   }
   else
   if(elapsedMS < 1000*30)
   {
      return 1000*5;
   }
   else
   if(elapsedMS < 1000*60)
   {
      return 1000*7;
   }
   else
   if(elapsedMS < 1000*300)
   {
      return 1000*10;
   }
   else
   {
      return 1000*30 + Random().getNextInRange(-2000, 2000);
   }
}
