#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/MapTargetsMsg.h>
#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/net/message/nodes/PublishCapacitiesMsg.h>
#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>
#include <common/net/message/nodes/RefreshTargetStatesMsg.h>
#include <common/net/message/nodes/RemoveNodeMsg.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/message/nodes/SetMirrorBuddyGroupMsg.h>
#include <common/net/message/nodes/storagepools/RefreshStoragePoolsMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SocketTk.h>
#include <program/Program.h>
#include "HbMgrNotification.h"


void HbMgrNotificationNodeAdded::processNotification()
{
   App* app = Program::getApp();

   NodeHandle node;

   if(nodeType == NODETYPE_Client)
      node = app->getClientNodes()->referenceNode(nodeNumID);
   else
      node = app->getServerStoreFromType(nodeType)->referenceNode(nodeNumID);

   if(!node)
      return;

   LOG_DBG(GENERAL, SPAM, "Propagating new node information.", node->getNodeIDWithTypeStr());


   propagateAddedNode(*node);
}

void HbMgrNotificationNodeAdded::propagateAddedNode(Node& node)
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreClients* clients = app->getClientNodes();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();

   NumNodeID nodeNumID = node.getNumID();
   NicAddressList nicList(node.getNicList() );
   const BitStore* nodeFeatureFlags = node.getNodeFeatures();


   if(nodeType == NODETYPE_Meta)
   {
      NumNodeID rootNodeID = app->getMetaNodes()->getRootNodeNumID();

      HeartbeatMsg hbMsg(node.getID(), nodeNumID, nodeType, &nicList, nodeFeatureFlags);
      hbMsg.setRootNumID(rootNodeID);
      hbMsg.setPorts(node.getPortUDP(), node.getPortTCP() );
      hbMsg.setFhgfsVersion(node.getFhgfsVersion() );

      dgramLis->sendToNodesUDPwithAck(clients, &hbMsg);
      dgramLis->sendToNodesUDPwithAck(metaNodes, &hbMsg);
   }
   else
   if(nodeType == NODETYPE_Storage)
   {
      HeartbeatMsg hbMsg(node.getID(), nodeNumID, nodeType, &nicList, nodeFeatureFlags);
      hbMsg.setPorts(node.getPortUDP(), node.getPortTCP() );
      hbMsg.setFhgfsVersion(node.getFhgfsVersion() );

      dgramLis->sendToNodesUDPwithAck(storageNodes, &hbMsg);
      dgramLis->sendToNodesUDPwithAck(clients, &hbMsg);
      dgramLis->sendToNodesUDPwithAck(metaNodes, &hbMsg);
   }
   else
   if(nodeType == NODETYPE_Client)
   {
      HeartbeatMsg hbMsg(node.getID(), nodeNumID, nodeType, &nicList, nodeFeatureFlags);
      hbMsg.setPorts(node.getPortUDP(), node.getPortTCP() );
      hbMsg.setFhgfsVersion(node.getFhgfsVersion() );

      dgramLis->sendToNodesUDPwithAck(metaNodes, &hbMsg);
   }
}

void HbMgrNotificationNodeRemoved::processNotification()
{
   LOG_DBG(GENERAL, SPAM, "Propagating removed node information.", nodeNumID, Node::nodeTypeToStr(nodeType));

   propagateRemovedNode();
}

void HbMgrNotificationNodeRemoved::propagateRemovedNode()
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreClients* clients = app->getClientNodes();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();

   RemoveNodeMsg removeMsg(nodeNumID, nodeType);

   if(nodeType == NODETYPE_Meta)
   {
      dgramLis->sendToNodesUDPwithAck(metaNodes, &removeMsg);
      dgramLis->sendToNodesUDPwithAck(clients, &removeMsg);
   }
   else
   if(nodeType == NODETYPE_Storage)
   {
      dgramLis->sendToNodesUDPwithAck(metaNodes, &removeMsg);
      dgramLis->sendToNodesUDPwithAck(clients, &removeMsg);
      dgramLis->sendToNodesUDPwithAck(storageNodes, &removeMsg);
   }
   else
   if(nodeType == NODETYPE_Client)
   {
      // Note: Meta and storage servers need to be informed about removed clients via their
      //    own downloadAndSyncNodes() method to handle the information in an appropriate thread
   }
}

void HbMgrNotificationTargetAdded::processNotification()
{
   LOG_DEBUG(__func__, Log_SPAM,
      "Propagating added target information: " + StringTk::uintToStr(targetID) + "; "
      "node: " + nodeID.str() );

   propagateAddedTarget();
}

void HbMgrNotificationTargetAdded::propagateAddedTarget()
{
   // note: we only submit a single target here to keep the UDP messages small

   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreClients* clients = app->getClientNodes();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();

   TargetPoolPairVec targetVec;
   targetVec.push_back(std::make_pair(targetID, storagePoolId));

   MapTargetsMsg msg(&targetVec, nodeID);

   dgramLis->sendToNodesUDPwithAck(storageNodes, &msg);
   dgramLis->sendToNodesUDPwithAck(clients, &msg);
   dgramLis->sendToNodesUDPwithAck(metaNodes, &msg);
}

void HbMgrNotificationRefreshCapacityPools::processNotification()
{
   LOG_DEBUG(__func__, Log_SPAM, std::string("Propagating capacity pools refresh order") );

   propagateRefreshCapacityPools();
}

void HbMgrNotificationRefreshCapacityPools::propagateRefreshCapacityPools()
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreServers* metaNodes = app->getMetaNodes();

   RefreshCapacityPoolsMsg msg;

   dgramLis->sendToNodesUDPwithAck(metaNodes, &msg);
}

void HbMgrNotificationRefreshTargetStates::processNotification()
{
   LOG_DEBUG(__func__, Log_SPAM, std::string("Propagating target states refresh order") );

   propagateRefreshTargetStates();
}

void HbMgrNotificationRefreshTargetStates::propagateRefreshTargetStates()
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   RefreshTargetStatesMsg msg;

   // send to clients
   NodeStoreClients* clientNodes = app->getClientNodes();
   dgramLis->sendToNodesUDPwithAck(clientNodes, &msg);

   // send to MDSs
   NodeStoreServers* metaNodes = app->getMetaNodes();
   dgramLis->sendToNodesUDPwithAck(metaNodes, &msg);

   // send to storage
   NodeStoreServers* storageNodes = app->getStorageNodes();
   dgramLis->sendToNodesUDPwithAck(storageNodes, &msg);
}

void HbMgrNotificationPublishCapacities::processNotification()
{
   LOG_DEBUG(__func__, Log_SPAM, std::string("Propagating target capacity publish order") );

   propagatePublishCapacities();
}

void HbMgrNotificationPublishCapacities::propagatePublishCapacities()
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   PublishCapacitiesMsg msg;

   // send to MDSs
   NodeStoreServers* metaNodes = app->getMetaNodes();
   dgramLis->sendToNodesUDPwithAck(metaNodes, &msg);

   // send to storage
   NodeStoreServers* storageNodes = app->getStorageNodes();
   dgramLis->sendToNodesUDPwithAck(storageNodes, &msg);
}

void HbMgrNotificationMirrorBuddyGroupAdded::processNotification()
{
   LOG_DEBUG(__func__, Log_SPAM,
      "Propagating added mirror buddy group information: " + StringTk::uintToStr(buddyGroupID)
      + "; targets: " + StringTk::uintToStr(primaryTargetID) + ","
      + StringTk::uintToStr(secondaryTargetID));

   propagateAddedMirrorBuddyGroup();
}

void HbMgrNotificationMirrorBuddyGroupAdded::propagateAddedMirrorBuddyGroup()
{
   // note: we only submit a single buddy group here to keep the UDP messages small
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   NodeStoreClients* clientNodes = app->getClientNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();

   SetMirrorBuddyGroupMsg msg(nodeType, primaryTargetID, secondaryTargetID, buddyGroupID, true);

   dgramLis->sendToNodesUDPwithAck(storageNodes, &msg);
   dgramLis->sendToNodesUDPwithAck(clientNodes, &msg);
   dgramLis->sendToNodesUDPwithAck(metaNodes, &msg);
}

void HbMgrNotificationRefreshStoragePools::processNotification()
{
   LOG_DBG(STORAGEPOOLS, SPAM, "Propagating storage target pool refresh order.");

   propagateRefreshStoragePools();
}

void HbMgrNotificationRefreshStoragePools::propagateRefreshStoragePools()
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();

   RefreshStoragePoolsMsg msg;

   dgramLis->sendToNodesUDPwithAck(storageNodes, &msg);
   dgramLis->sendToNodesUDPwithAck(metaNodes, &msg);
}
