#include <toolkit/NoAllocBufferStore.h>
#include <common/net/message/nodes/GetMirrorBuddyGroupsMsg.h>
#include <common/net/message/nodes/GetMirrorBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetNodesMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/nodes/GetTargetStatesRespMsg.h>
#include <common/threading/Thread.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/Random.h>
#include "NodesTk.h"

/**
 * Download node list from given source node.
 *
 * @param sourceNode the node from which node you want to download
 * @param nodeType which type of node list you want to download
 * @param outNodeList caller is responsible for the deletion of the received nodes
 * @param outRootID may be NULL if caller is not interested
 * @param outRootIsBuddyMirrored may be NULL if caller is not interested
 * @return true if download successful
 */
bool NodesTk_downloadNodes(App* app, Node* sourceNode, NodeType nodeType, NodeList* outNodeList,
   NumNodeID* outRootNodeID, bool* outRootIsBuddyMirrored)
{
   bool retVal = false;

   GetNodesMsg msg;

   FhgfsOpsErr commRes;
   GetNodesRespMsg* respMsgCast;
   RequestResponseArgs rrArgs;

   // prepare request
   GetNodesMsg_initFromValue(&msg, nodeType);
   RequestResponseArgs_prepare(&rrArgs, sourceNode, (NetMessage*)&msg, NETMSGTYPE_GetNodesResp);

#ifndef BEEGFS_DEBUG
   // Silence log message unless built in debug mode.
   rrArgs.logFlags |= ( REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                      | REQUESTRESPONSEARGS_LOGFLAG_RETRY );
#endif // BEEGFS_DEBUG

   // connect & communicate

   commRes = MessagingTk_requestResponseWithRRArgs(app, &rrArgs);

   if(unlikely(commRes != FhgfsOpsErr_SUCCESS) )
      goto cleanup_request;

   // handle result
   respMsgCast = (GetNodesRespMsg*)rrArgs.outRespMsg;

   GetNodesRespMsg_parseNodeList(app, respMsgCast, outNodeList);

   if(outRootNodeID)
      *outRootNodeID = GetNodesRespMsg_getRootNumID(respMsgCast);

   if (outRootIsBuddyMirrored)
      *outRootIsBuddyMirrored = GetNodesRespMsg_getRootIsBuddyMirrored(respMsgCast);

   retVal = true;

   // cleanup

   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

/**
 * Downloads target mappings from given source node.
 *
 * @return true if download successful
 */
bool NodesTk_downloadTargetMappings(App* app, Node* sourceNode, UInt16List* outTargetIDs,
   NumNodeIDList* outNodeIDs)
{
   bool retVal = false;

   GetTargetMappingsMsg msg;

   FhgfsOpsErr commRes;
   GetTargetMappingsRespMsg* respMsgCast;
   RequestResponseArgs rrArgs;

   // prepare request
   GetTargetMappingsMsg_init(&msg);
   RequestResponseArgs_prepare(&rrArgs, sourceNode, (NetMessage*)&msg,
      NETMSGTYPE_GetTargetMappingsResp);

#ifndef BEEGFS_DEBUG
   // Silence log message unless built in debug mode.
   rrArgs.logFlags |= ( REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                      | REQUESTRESPONSEARGS_LOGFLAG_RETRY );
#endif // BEEGFS_DEBUG

   // communicate

   commRes = MessagingTk_requestResponseWithRRArgs(app, &rrArgs);

   if(unlikely(commRes != FhgfsOpsErr_SUCCESS) )
      goto cleanup_request;

   // handle result

   respMsgCast = (GetTargetMappingsRespMsg*)rrArgs.outRespMsg;

   GetTargetMappingsRespMsg_parseTargetIDs(respMsgCast, outTargetIDs);
   GetTargetMappingsRespMsg_parseNodeIDs(respMsgCast, outNodeIDs);

   if(unlikely(
      UInt16List_length(outTargetIDs) != NumNodeIDList_length(outNodeIDs) ) )
   { // list sizes do not match
      printk_fhgfs(KERN_WARNING, "%s:%d: List sizes do not match. Out of memory?\n",
         __func__, __LINE__);

      // (note: caller has to cleanup lists anyways, so we don't do it here)
      retVal = false;
   }
   else
      retVal = true;

   // cleanup

   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

bool NodesTk_downloadMirrorBuddyGroups(App* app, Node* sourceNode, NodeType nodeType,
   UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs, UInt16List* outSecondaryTargetIDs)
{
   bool retVal = false;

   NoAllocBufferStore* bufStore = App_getMsgBufStore(app);

   GetMirrorBuddyGroupsMsg msg;

   FhgfsOpsErr commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   GetMirrorBuddyGroupsRespMsg* respMsgCast;

   // prepare request
   GetMirrorBuddyGroupsMsg_init(&msg, nodeType);

   // communicate

   commRes = MessagingTk_requestResponse(app,
      sourceNode, (NetMessage*)&msg, NETMSGTYPE_GetMirrorBuddyGroupsResp, &respBuf, &respMsg);

   if(unlikely(commRes != FhgfsOpsErr_SUCCESS) )
      goto cleanup_request;

   // handle result

   respMsgCast = (GetMirrorBuddyGroupsRespMsg*)respMsg;

   GetMirrorBuddyGroupsRespMsg_parseBuddyGroupIDs(respMsgCast, outBuddyGroupIDs);
   GetMirrorBuddyGroupsRespMsg_parsePrimaryTargetIDs(respMsgCast, outPrimaryTargetIDs);
   GetMirrorBuddyGroupsRespMsg_parseSecondaryTargetIDs(respMsgCast, outSecondaryTargetIDs);

   if(unlikely(
      (UInt16List_length(outBuddyGroupIDs) != UInt16List_length(outPrimaryTargetIDs) ) ||
      (UInt16List_length(outBuddyGroupIDs) != UInt16List_length(outSecondaryTargetIDs) ) ) )
   { // list sizes do not match
      printk_fhgfs(KERN_WARNING, "%s:%d: List sizes do not match. Out of memory?\n",
         __func__, __LINE__);

      // (note: caller has to cleanup lists anyways, so we don't do it here)
      retVal = false;
   }
   else
      retVal = true;

   // cleanup

   NETMESSAGE_FREE(respMsg);
   NoAllocBufferStore_addBuf(bufStore, respBuf);

cleanup_request:
   return retVal;
}

/**
 * Download target states and buddy groups combined in a single message.
 */
bool NodesTk_downloadStatesAndBuddyGroups(App* app, Node* sourceNode, NodeType nodeType,
   UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs, UInt16List* outSecondaryTargetIDs,
   UInt16List* outTargetIDs, UInt8List* outTargetReachabilityStates,
   UInt8List* outTargetConsistencyStates)
{
   bool retVal = false;

   GetStatesAndBuddyGroupsMsg msg;

   FhgfsOpsErr commRes;
   GetStatesAndBuddyGroupsRespMsg* respMsgCast;
   RequestResponseArgs rrArgs;

   // prepare request
   GetStatesAndBuddyGroupsMsg_init(&msg, nodeType);
   RequestResponseArgs_prepare(&rrArgs, sourceNode, (NetMessage*)&msg,
      NETMSGTYPE_GetStatesAndBuddyGroupsResp);

#ifndef BEEGFS_DEBUG
   // Silence log message unless built in debug mode.
   rrArgs.logFlags |= ( REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                      | REQUESTRESPONSEARGS_LOGFLAG_RETRY );
#endif // BEEGFS_DEBUG

   // communicate

   commRes = MessagingTk_requestResponseWithRRArgs(app, &rrArgs);

   if(unlikely(commRes != FhgfsOpsErr_SUCCESS) )
      goto cleanup_request;

   // handle result

   respMsgCast = (GetStatesAndBuddyGroupsRespMsg*)rrArgs.outRespMsg;

   GetStatesAndBuddyGroupsRespMsg_parseBuddyGroupIDs(respMsgCast, outBuddyGroupIDs);
   GetStatesAndBuddyGroupsRespMsg_parsePrimaryTargetIDs(respMsgCast, outPrimaryTargetIDs);
   GetStatesAndBuddyGroupsRespMsg_parseSecondaryTargetIDs(respMsgCast, outSecondaryTargetIDs);

   GetStatesAndBuddyGroupsRespMsg_parseTargetIDs(respMsgCast, outTargetIDs);
   GetStatesAndBuddyGroupsRespMsg_parseReachabilityStates(respMsgCast, outTargetReachabilityStates);
   GetStatesAndBuddyGroupsRespMsg_parseConsistencyStates(respMsgCast, outTargetConsistencyStates);

   if(unlikely(
      (UInt16List_length(outBuddyGroupIDs) != UInt16List_length(outPrimaryTargetIDs) ) ||
      (UInt16List_length(outBuddyGroupIDs) != UInt16List_length(outSecondaryTargetIDs) ) ||
      (UInt16List_length(outTargetIDs) != UInt8List_length(outTargetReachabilityStates) ) ||
      (UInt16List_length(outTargetIDs) != UInt8List_length(outTargetConsistencyStates) ) ) )
   { // list sizes do not match
      printk_fhgfs(KERN_WARNING, "%s:%d: List sizes do not match. Out of memory?\n",
         __func__, __LINE__);

      // (note: caller has to cleanup lists anyways, so we don't do it here)
      retVal = false;
   }
   else
      retVal = true;

   // cleanup

   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}


/**
 * Walk over all nodes in the given store and drop established (available) connections.
 *
 * @return number of dropped connections
 */
unsigned NodesTk_dropAllConnsByStore(NodeStoreEx* nodes)
{
   unsigned numDroppedConns = 0;

   Node* node = NodeStoreEx_referenceFirstNode(nodes);
   while(node)
   {
      NodeConnPool* connPool = Node_getConnPool(node);

      numDroppedConns += NodeConnPool_disconnectAvailableStreams(connPool);

      node = NodeStoreEx_referenceNextNodeAndReleaseOld(nodes, node); // iterate to next node
   }

   return numDroppedConns;
}
