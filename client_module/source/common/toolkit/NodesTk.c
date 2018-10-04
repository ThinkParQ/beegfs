#include <toolkit/NoAllocBufferStore.h>
#include <common/net/message/nodes/GetMirrorBuddyGroupsMsg.h>
#include <common/net/message/nodes/GetNodesMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
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
bool NodesTk_downloadTargetMappings(App* app, Node* sourceNode, struct list_head* mappings)
{
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
      return false;

   // handle result

   respMsgCast = (GetTargetMappingsRespMsg*)rrArgs.outRespMsg;

   list_splice_tail_init(&respMsgCast->mappings, mappings);

   // cleanup

   RequestResponseArgs_freeRespBuffers(&rrArgs, app);
   return true;
}

/**
 * Download target states and buddy groups combined in a single message.
 */
bool NodesTk_downloadStatesAndBuddyGroups(App* app, Node* sourceNode, NodeType nodeType,
   struct list_head* groups, struct list_head* states)
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

   list_splice_tail_init(&respMsgCast->groups, groups);
   list_splice_tail_init(&respMsgCast->states, states);
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
