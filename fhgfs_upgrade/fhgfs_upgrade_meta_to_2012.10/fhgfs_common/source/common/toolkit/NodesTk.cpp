#include <common/net/message/nodes/GetNodesMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/threading/PThread.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/SocketTk.h>
#include "NodesTk.h"


/**
 * Note: Will block as long as it takes to catch a heartbeat from the mgmt host (or until
 *    a thread termination order is received) 
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
   unsigned msgLen = msg.getMsgLength();
   char* msgBuf = MessagingTk::createMsgBuf(&msg);
   

   // wait for mgmt node to appear and periodically resend request
   while(!currentThread || !currentThread->getSelfTerminate() )
   {
      if(lastRetryTime.elapsedMS() >= nextRetryDelayMS)
      { // time to send request again
         bool getHostByNameRes = SocketTk::getHostByName(hostname.c_str(), &ipAddr);
         if(getHostByNameRes)
            dgramLis->sendto(msgBuf, msgLen, 0, ipAddr, portUDP);

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
   
   free(msgBuf);
   
   return gotMgmtHeartbeat;
}

/**
 * Downloads node list from given sourceNode.
 * 
 * @param sourceNode the node from which node you want to download
 * @param nodeType which type of node list you want to download
 * @param outNodeList caller is responsible for the deletion of the received nodes
 * @param outRootNumID numeric ID of root mds, may be NULL if caller is not interested
 * @return true if download successful
 */
bool NodesTk::downloadNodes(Node* sourceNode, NodeType nodeType, NodeList* outNodeList,
   uint16_t* outRootNumID)
{
   bool retVal = false;
   
   GetNodesMsg msg(nodeType);
   
   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   GetNodesRespMsg* respMsgCast;

   // connect & communicate
   commRes = MessagingTk::requestResponse(
      sourceNode, &msg, NETMSGTYPE_GetNodesResp, &respBuf, &respMsg);
   if(!commRes)
      goto err_exit;
   
   // handle result
   respMsgCast = (GetNodesRespMsg*)respMsg;

   respMsgCast->parseNodeList(outNodeList);
   
   if(outRootNumID)
      *outRootNumID = respMsgCast->getRootNumID();
   
   retVal = true;

   // cleanup
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

err_exit:
   
   return retVal;
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
bool NodesTk::downloadTargetMappings(Node* sourceNode, UInt16List* outTargetIDs,
   UInt16List* outNodeIDs)
{
   bool retVal = false;

   GetTargetMappingsMsg msg;

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   GetTargetMappingsRespMsg* respMsgCast;

   // connect & communicate
   commRes = MessagingTk::requestResponse(
      sourceNode, &msg, NETMSGTYPE_GetTargetMappingsResp, &respBuf, &respMsg);
   if(!commRes)
      goto err_exit;

   // handle result
   respMsgCast = (GetTargetMappingsRespMsg*)respMsg;

   respMsgCast->parseTargetIDs(outTargetIDs);
   respMsgCast->parseNodeIDs(outNodeIDs);

   retVal = true;

   // cleanup
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

err_exit:

   return retVal;
}

/**
 * Node: This is probably only useful in very rare cases, because normally you would want to
 *    use NodeStore::syncNodes() instead of this add-only method.
 * 
 * @param nodeList All the nodes will belong to the store afterwards, so don't use them anymore
 * after calling this method.
 */
void NodesTk::moveNodesFromListToStore(NodeList* nodeList, AbstractNodeStore* nodeStore)
{
   for(NodeListIter iter = nodeList->begin(); iter != nodeList->end(); iter++)
   {
      Node* currentNode = *iter;
      
      nodeStore->addOrUpdateNode(&currentNode);
   }
   
   nodeList->clear();
}

/**
 * Deletes all nodes in the list, but not the list itself.
 */
void NodesTk::deleteListNodes(NodeList* nodeList)
{
   for(NodeListIter iter = nodeList->begin(); iter != nodeList->end(); iter++)
   {
      Node* currentNode = *iter;

      delete(currentNode);
   }

   nodeList->clear();
}

/**
 * Copy and append all nodes from sourceList to outDestList.
 *
 * @outDestList all nodes are new'ed, so the caller is responsible for cleanup
 */
void NodesTk::copyListNodes(NodeList* sourceList, NodeList* outDestList)
{
   for(NodeListIter iter = sourceList->begin(); iter != sourceList->end(); iter++)
   {
      Node* currentNode = *iter;

      NicAddressList currentNicList = currentNode->getNicList();

      Node* newNode = new Node(currentNode->getID(), currentNode->getNumID(),
         currentNode->getPortUDP(), currentNode->getPortTCP(), currentNicList);

      outDestList->push_back(newNode);
   }
}

/**
 * Note: Useful if you got the list e.g. via downloadNodes(), because that doesn't set the
 * localNicCaps.
 * Note: Make sure you didn't use the connPools of the nodes before calling this.
 */
void NodesTk::applyLocalNicCapsToList(Node* localNode, NodeList* nodeList)
{
   for(NodeListIter iter = nodeList->begin(); iter != nodeList->end(); iter++)
   {
      Node* currentNode = *iter;

      // set local nic capabilities
      NicAddressList localNicList(localNode->getNicList() );
      NicListCapabilities localNicCaps;

      NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
      currentNode->getConnPool()->setLocalNicCaps(&localNicCaps);
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
