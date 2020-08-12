#include <app/App.h>
#include <common/nodes/Node.h>
#include <common/toolkit/SocketTk.h>
#include <common/net/msghelpers/MsgHelperAck.h>
#include <common/toolkit/ListTk.h>
#include <nodes/NodeStoreEx.h>
#include <app/config/Config.h>
#include "HeartbeatMsgEx.h"

const struct NetMessageOps HeartbeatMsgEx_Ops = {
   .serializePayload = HeartbeatMsgEx_serializePayload,
   .deserializePayload = HeartbeatMsgEx_deserializePayload,
   .processIncoming = __HeartbeatMsgEx_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void HeartbeatMsgEx_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   HeartbeatMsgEx* thisCast = (HeartbeatMsgEx*)this;

   // instanceVersion
   Serialization_serializeUInt64(ctx, thisCast->instanceVersion);

   // nicListVersion
   Serialization_serializeUInt64(ctx, thisCast->nicListVersion);

   // nodeType
   Serialization_serializeInt(ctx, thisCast->nodeType);

   // nodeID
   Serialization_serializeStr(ctx, thisCast->nodeIDLen, thisCast->nodeID);

   // ackID
   Serialization_serializeStrAlign4(ctx, thisCast->ackIDLen, thisCast->ackID);

   // nodeNumID
   NumNodeID_serialize(ctx, &thisCast->nodeNumID);

   // rootNumID
   NumNodeID_serialize(ctx, &thisCast->rootNumID);

   // rootIsBuddyMirrored
   Serialization_serializeBool(ctx, thisCast->rootIsBuddyMirrored);

   // portUDP
   Serialization_serializeUShort(ctx, thisCast->portUDP);

   // portTCP
   Serialization_serializeUShort(ctx, thisCast->portTCP);

   // nicList
   Serialization_serializeNicList(ctx, thisCast->nicList);
}

bool HeartbeatMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   HeartbeatMsgEx* thisCast = (HeartbeatMsgEx*)this;

   // instanceVersion
   if(!Serialization_deserializeUInt64(ctx, &thisCast->instanceVersion) )
      return false;

   // nicListVersion
   if(!Serialization_deserializeUInt64(ctx, &thisCast->nicListVersion) )
      return false;

   // nodeType
   if(!Serialization_deserializeInt(ctx, &thisCast->nodeType) )
      return false;

   // nodeID
   if(!Serialization_deserializeStr(ctx, &thisCast->nodeIDLen, &thisCast->nodeID) )
      return false;

   // ackID
   if(!Serialization_deserializeStrAlign4(ctx, &thisCast->ackIDLen, &thisCast->ackID) )
      return false;

   // nodeNumID
   if(!NumNodeID_deserialize(ctx, &thisCast->nodeNumID) )
      return false;

   // rootNumID
   if(!NumNodeID_deserialize(ctx, &thisCast->rootNumID) )
      return false;

   // rootIsBuddyMirrored
   if(!Serialization_deserializeBool(ctx, &thisCast->rootIsBuddyMirrored) )
      return false;

   // portUDP
   if(!Serialization_deserializeUShort(ctx, &thisCast->portUDP) )
      return false;

   // portTCP
   if(!Serialization_deserializeUShort(ctx, &thisCast->portTCP) )
      return false;

   // nicList
   if(!Serialization_deserializeNicListPreprocess(ctx, &thisCast->rawNicList) )
      return false;

   return true;
}

bool __HeartbeatMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Heartbeat incoming";

   HeartbeatMsgEx* thisCast = (HeartbeatMsgEx*)this;

   NicAddressList nicList;
   Node* node;
   NodeConnPool* connPool;
   Node* localNode;
   NicAddressList* localNicList;
   NicListCapabilities localNicCaps;
   NodeStoreEx* nodes = NULL;
   bool isNodeNew;
   NumNodeID nodeNumID;

   // check if nodeNumID is set
   nodeNumID = HeartbeatMsgEx_getNodeNumID(thisCast);
   if(NumNodeID_isZero(&nodeNumID))
   { // shouldn't happen: this node would need to register first to get a nodeNumID assigned
      Logger_logFormatted(log, Log_WARNING, logContext,
         "Rejecting heartbeat of node without numeric ID: %s (Type: %s)",
         HeartbeatMsgEx_getNodeID(thisCast),
         Node_nodeTypeToStr(HeartbeatMsgEx_getNodeType(thisCast) ) );

      goto ack_resp;
   }

   // find the corresponding node store for this node type

   switch(HeartbeatMsgEx_getNodeType(thisCast) )
   {
      case NODETYPE_Meta:
         nodes = App_getMetaNodes(app); break;

      case NODETYPE_Storage:
         nodes = App_getStorageNodes(app); break;

      case NODETYPE_Mgmt:
         nodes = App_getMgmtNodes(app); break;

      default:
      {
         int nodeType = HeartbeatMsgEx_getNodeType(thisCast);
         const char* nodeID = HeartbeatMsgEx_getNodeID(thisCast);

         Logger_logErrFormatted(log, logContext, "Invalid node type: %d (%s); ID: %s",
            nodeType, Node_nodeTypeToStr(nodeType), nodeID);

         goto ack_resp;
      } break;
   }

   // construct node

   NicAddressList_init(&nicList);

   HeartbeatMsgEx_parseNicList(thisCast, &nicList);

   node = Node_construct(app,
      HeartbeatMsgEx_getNodeID(thisCast), HeartbeatMsgEx_getNodeNumID(thisCast),
      HeartbeatMsgEx_getPortUDP(thisCast), HeartbeatMsgEx_getPortTCP(thisCast), &nicList);
      // (will belong to the NodeStore => no destruct() required)

   Node_setNodeType(node, HeartbeatMsgEx_getNodeType(thisCast) );

   // set local nic capabilities

   localNode = App_getLocalNode(app);
   localNicList = Node_getNicList(localNode);
   connPool = Node_getConnPool(node);

   NIC_supportedCapabilities(localNicList, &localNicCaps);
   NodeConnPool_setLocalNicCaps(connPool, &localNicCaps);

   // add node to store (or update it)

   isNodeNew = NodeStoreEx_addOrUpdateNode(nodes, &node);
   if(isNodeNew)
   {
      bool supportsSDP = NIC_supportsSDP(&nicList);
      bool supportsRDMA = NIC_supportsRDMA(&nicList);

      Logger_logFormatted(log, Log_WARNING, logContext,
         "New node: %s %s [ID: %hu]; %s%s",
         Node_nodeTypeToStr(HeartbeatMsgEx_getNodeType(thisCast) ),
         HeartbeatMsgEx_getNodeID(thisCast),
         HeartbeatMsgEx_getNodeNumID(thisCast).value,
         (supportsSDP ? "SDP; " : ""),
         (supportsRDMA ? "RDMA; " : "") );
   }

   __HeartbeatMsgEx_processIncomingRoot(thisCast, app);

ack_resp:
   // send ack
   MsgHelperAck_respondToAckRequest(app, HeartbeatMsgEx_getAckID(thisCast), fromAddr, sock,
      respBuf, bufLen);

   // clean-up
   ListTk_kfreeNicAddressListElems(&nicList);
   NicAddressList_uninit(&nicList);

   return true;
}

/**
 * Handles the contained root information.
 */
void __HeartbeatMsgEx_processIncomingRoot(HeartbeatMsgEx* this, App* app)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Heartbeat incoming (root)";

   NodeStoreEx* metaNodes;
   bool setRootRes;
   NodeOrGroup rootOwner = this->rootIsBuddyMirrored
      ? NodeOrGroup_fromGroup(this->rootNumID.value)
      : NodeOrGroup_fromNode(this->rootNumID);
   NumNodeID rootNumID = HeartbeatMsgEx_getRootNumID(this);

   // check whether root info is defined
   if( (HeartbeatMsgEx_getNodeType(this) != NODETYPE_Meta) || (NumNodeID_isZero(&rootNumID)))
      return;

   // try to apply the contained root info

   metaNodes = App_getMetaNodes(app);

   setRootRes = NodeStoreEx_setRootOwner(metaNodes, rootOwner, false);

   if(setRootRes)
   { // found the very first root
      Logger_logFormatted(log, Log_CRITICAL, logContext, "Root (by Heartbeat): %hu",
         HeartbeatMsgEx_getRootNumID(this).value );
   }

}

