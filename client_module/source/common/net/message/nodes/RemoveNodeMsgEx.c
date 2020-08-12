#include <common/nodes/Node.h>
#include <components/DatagramListener.h>
#include <nodes/NodeStoreEx.h>
#include <common/toolkit/SocketTk.h>
#include <common/net/msghelpers/MsgHelperAck.h>
#include <app/App.h>
#include "RemoveNodeRespMsg.h"
#include "RemoveNodeMsgEx.h"


const struct NetMessageOps RemoveNodeMsgEx_Ops = {
   .serializePayload = RemoveNodeMsgEx_serializePayload,
   .deserializePayload = RemoveNodeMsgEx_deserializePayload,
   .processIncoming = __RemoveNodeMsgEx_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void RemoveNodeMsgEx_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   RemoveNodeMsgEx* thisCast = (RemoveNodeMsgEx*)this;

   // nodeType
   Serialization_serializeShort(ctx, thisCast->nodeType);

   // nodeNumID
   NumNodeID_serialize(ctx, &thisCast->nodeNumID);

   // ackID
   Serialization_serializeStr(ctx, thisCast->ackIDLen, thisCast->ackID);
}

bool RemoveNodeMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   RemoveNodeMsgEx* thisCast = (RemoveNodeMsgEx*)this;

   // nodeType
   if(!Serialization_deserializeShort(ctx, &thisCast->nodeType) )
      return false;

   // nodeNumID
   if(!NumNodeID_deserialize(ctx, &thisCast->nodeNumID) )
      return false;

   // ackID
   if(!Serialization_deserializeStr(ctx, &thisCast->ackIDLen, &thisCast->ackID) )
      return false;

   return true;
}

bool __RemoveNodeMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "RemoveNodeMsg incoming";

   RemoveNodeMsgEx* thisCast = (RemoveNodeMsgEx*)this;

   const char* peer;
   NodeType nodeType = (NodeType)RemoveNodeMsgEx_getNodeType(thisCast);
   NumNodeID nodeID = RemoveNodeMsgEx_getNodeNumID(thisCast);

   RemoveNodeRespMsg respMsg;
   unsigned respLen;
   bool serializeRes;
   ssize_t sendRes;

   peer = fromAddr ?
      SocketTk_ipaddrToStr(&fromAddr->addr) : StringTk_strDup(Socket_getPeername(sock) );
   LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
      "Received a RemoveNodeMsg from: %s; Node: %s %hu",
      peer, Node_nodeTypeToStr(nodeType), nodeID.value);
   kfree(peer);


   if(nodeType == NODETYPE_Meta)
   {
      NodeStoreEx* nodes = App_getMetaNodes(app);
      if(NodeStoreEx_deleteNode(nodes, nodeID) )
         Logger_logFormatted(log, Log_WARNING, logContext, "Removed metadata node: %hu", nodeID.value);
   }
   else
   if(nodeType == NODETYPE_Storage)
   {
      NodeStoreEx* nodes = App_getStorageNodes(app);
      if(NodeStoreEx_deleteNode(nodes, nodeID) )
         Logger_logFormatted(log, Log_WARNING, logContext, "Removed storage node: %hu", nodeID.value);
   }
   else
   { // should never happen
      Logger_logFormatted(log, Log_WARNING, logContext,
         "Received removal request for invalid node type: %d (%s)",
         (int)nodeType, Node_nodeTypeToStr(nodeType) );
   }


   // send response
   if(!MsgHelperAck_respondToAckRequest(app, RemoveNodeMsgEx_getAckID(thisCast),
      fromAddr, sock, respBuf, bufLen) )
   {
      RemoveNodeRespMsg_initFromValue(&respMsg, 0);

      respLen = NetMessage_getMsgLength( (NetMessage*)&respMsg);
      serializeRes = NetMessage_serialize( (NetMessage*)&respMsg, respBuf, bufLen);
      if(unlikely(!serializeRes) )
      {
         Logger_logErrFormatted(log, logContext, "Unable to serialize response");
         goto err_resp_uninit;
      }

      if(fromAddr)
      { // datagram => sync via dgramLis send method
         DatagramListener* dgramLis = App_getDatagramListener(app);
         sendRes = DatagramListener_sendto(dgramLis, respBuf, respLen, 0, fromAddr);
      }
      else
         sendRes = Socket_sendto(sock, respBuf, respLen, 0, NULL);

      if(unlikely(sendRes <= 0) )
         Logger_logErrFormatted(log, logContext, "Send error. ErrCode: %lld", (long long)sendRes);
   }

err_resp_uninit:
   return true;
}

