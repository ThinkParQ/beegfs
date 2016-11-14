#include <common/net/message/nodes/RegisterNodeRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/VersionTk.h>
#include <program/Program.h>
#include "RegisterNodeMsgEx.h"


bool RegisterNodeMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RegisterNodeMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a RegisterNodeMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   NodeType nodeType = getNodeType();
   std::string nodeID(getNodeID() );

   NodeHandle node;

   NumNodeID newNodeNumID;
   bool isNodeNew;


   LOG_DEBUG_CONTEXT(log, Log_SPAM,
      "Type: " + Node::nodeTypeToStr(nodeType) + "; "
      "NodeID: " + nodeID + "; "
      "NodeNumID: " + getNodeNumID().str() );


   // check for empty nodeID; (sanity check, should never fail)

   if(unlikely(nodeID.empty() ) )
   {
      log.log(Log_WARNING, "Rejecting registration of node with empty string ID "
         "from: " + ctx.peerName()  + "; "
         "type: " + Node::nodeTypeToStr(nodeType) );

      return false;
   }


   // get the corresponding node store for this node type
/*
   NodeStoreServers* servers = app->getServerStoreFromType(nodeType);
   if(unlikely(!servers) )
   {
      log.logErr("Invalid node type for registration: " + Node::nodeTypeToStr(nodeType) + "; "
         "from: " + peer);

      newNodeNumID = 0;
      goto send_response;
   }
*/

   AbstractNodeStore* nodeStore = NULL;

   if (nodeType == NODETYPE_Client)
      nodeStore = app->getClientNodes();
   else
      nodeStore = app->getServerStoreFromType(nodeType);

   if(unlikely(!nodeStore) )
   {
      log.logErr("Invalid node type for registration: " + Node::nodeTypeToStr(nodeType) + "; "
         "from: " + ctx.peerName() );

      newNodeNumID = NumNodeID();
      goto send_response;
   }

   // check if adding of new servers is allowed
   if(!checkNewServerAllowed(nodeStore, getNodeNumID(), nodeType) )
   { // this is a new server and adding was disabled
      log.log(Log_WARNING, "Registration of new servers disabled. Rejecting node: " +
         nodeID + " (Type: " + Node::nodeTypeToStr(nodeType) + ")");

      newNodeNumID = NumNodeID();
      goto send_response;
   }

   // construct node

   node = constructNode(nodeID, getNodeNumID(), getPortUDP(), getPortTCP(), getNicList() );

   node->setNodeType(getNodeType() );
   node->setFhgfsVersion(getFhgfsVersion() );

   node->setFeatureFlags(&getNodeFeatureFlags() );

   // add node to store (or update it)

   isNodeNew = nodeStore->addOrUpdateNodeEx(std::move(node), &newNodeNumID);

   if(!newNodeNumID)
   { // unable to add node to store
      log.log(Log_WARNING,
         "Unable to add node with given numeric ID: " + getNodeNumID().str() + "; "
         "string ID: " + getNodeID() + "; "
         "Type: " + Node::nodeTypeToStr(nodeType) );

      goto send_response;
   }

   /* note on capacity pools:
      we may not add new nodes to capacity pools here yet, because at this point the registered
      node is not ready to receive messages yet. so we will add it to capacity pools later when we
      receive its first heartbeat msg. */

   processIncomingRoot(getRootNumID(), nodeType, getRootIsBuddyMirrored());

   if(isNodeNew)
   { // this node is new
      processNewNode(nodeID, newNodeNumID, nodeType, getFhgfsVersion(), &getNicList(),
         ctx.peerName() );
   }

send_response:
   ctx.sendResponse(RegisterNodeRespMsg(newNodeNumID) );

   return true;
}



/**
 * Checks whether this is a heartbeat/registration from a new (i.e. not yet registered) server that
 * should be rejected.
 *
 * @return false if this is a heartbeat from a new server and adding of new servers has been
 * disabled, true otherwise.
 */
bool RegisterNodeMsgEx::checkNewServerAllowed(AbstractNodeStore* nodeStore, NumNodeID nodeNumID,
   NodeType nodeType)
{
   bool isAllowed = true;

   switch(nodeType)
   {
      case NODETYPE_Meta:
      case NODETYPE_Storage:
      case NODETYPE_Mgmt:
      {
         NodeStoreServers* servers = (NodeStoreServers*)nodeStore;
         Config* cfg = Program::getApp()->getConfig();
         bool newServersAllowed = cfg->getSysAllowNewServers();

         if(!newServersAllowed &&
            ( (nodeNumID == 0) || (!servers->isNodeActive(nodeNumID)) ) )
            isAllowed = false;
      } break;

      default:
      { /* most certainly a client (we assume invalid node types are handled by the caller, so we
           don't check for invalid/unknown node type here) */
      } break;
   }

   return isAllowed;
}

/**
 * Instantiate new node object and intialize it properly
 */
std::shared_ptr<Node> RegisterNodeMsgEx::constructNode(std::string nodeID, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList& nicList)
{
   App* app = Program::getApp();

   // alloc node

   auto node = std::make_shared<Node>(nodeID, nodeNumID, portUDP, portTCP, nicList);

   // set local nic capabilities

   NicAddressList localNicList(app->getLocalNicList() );
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   node->getConnPool()->setLocalNicCaps(&localNicCaps);

   return node;
}

/**
 * Handles the contained root information in a MDS heartbeat.
 *
 * @param nodeType the type of node that sent the rootNumID
 */
void RegisterNodeMsgEx::processIncomingRoot(NumNodeID rootNumID, NodeType nodeType,
   bool rootIsBuddyMirrored)
{
   /* Note: Be careful not to call this before we actually added the node (if possible), because
      we will try to send a corresponding root heartbeat to the other nodes if the root node can
      be referenced from the store. */

   // check whether root info is defined
   if(nodeType != NODETYPE_Meta)
      return;

   // try to apply the contained root info
   Program::getApp()->getHeartbeatMgr()->initRootNode(rootNumID, rootIsBuddyMirrored);
}

/**
 * "Post-processing" of newly added nodes: pring log msg, notify other nodes, ...
 */
void RegisterNodeMsgEx::processNewNode(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType,
   unsigned fhgfsVersion, NicAddressList* nicList, std::string sourcePeer)
{
   LogContext log("Node registration");

   App* app = Program::getApp();
   HeartbeatManager* heartbeatMgr = app->getHeartbeatMgr();
   InternodeSyncer* internodeSyncer = app->getInternodeSyncer();

   // print node info to log

   bool supportsSDP = NetworkInterfaceCard::supportsSDP(nicList);
   bool supportsRDMA = NetworkInterfaceCard::supportsRDMA(nicList);
   std::string nodeTypeStr = Node::nodeTypeToStr(nodeType);

   std::string nodeIDWithTypeStr = Node::getNodeIDWithTypeStr(nodeID, nodeNumID, nodeType);

   std::string fhgfsPseudoVersionStr = VersionTk::versionCodeToPseudoVersionStr(fhgfsVersion);


   log.log(Log_WARNING, std::string("New node: ") +
      nodeIDWithTypeStr + "; " +
      std::string(supportsSDP ? "SDP; " : "") +
      std::string(supportsRDMA ? "RDMA; " : "") +
      std::string("Ver: ") + fhgfsPseudoVersionStr  + "; " +
      std::string("Source: ") + sourcePeer);

   log.log(Log_DEBUG, std::string("Number of nodes: ") +
      "Meta: " + StringTk::uintToStr(app->getMetaNodes()->getSize() ) + "; "
      "Storage: " + StringTk::uintToStr(app->getStorageNodes()->getSize() ) + "; "
      "Client: " + StringTk::uintToStr(app->getClientNodes()->getSize() ) + "; "
      "Mgmt: " + StringTk::uintToStr(app->getMgmtNodes()->getSize() ) );


   // new node => inform others about the new one

   heartbeatMgr->notifyAsyncAddedNode(nodeID, nodeNumID, nodeType);


   // new server => update capacity pools

   if( (nodeType == NODETYPE_Meta) || (nodeType == NODETYPE_Storage) )
      internodeSyncer->setForcePoolsUpdate();
}

