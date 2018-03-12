#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <program/Program.h>
#include "GetNodesMsgEx.h"

bool GetNodesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetNodes incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a GetNodesMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   NodeType nodeType = getNodeType();

   LOG_DEBUG_CONTEXT(log, Log_SPAM, std::string("NodeType: ") + Node::nodeTypeToStr(nodeType) );


   // get corresponding node store

   AbstractNodeStore* nodes = app->getAbstractNodeStoreFromType(nodeType);
   if(!nodes)

   {
      LOG(ERR, "Invalid node type.",
            as("Node Type", Node::nodeTypeToStr(nodeType)),
            as("Sender", ctx.peerName())
         );
      return false;
   }

   // get root ID

   NumNodeID rootNumID;
   bool rootIsBuddyMirrored = false;

   if(nodeType == NODETYPE_Meta)
   {
      rootNumID = app->getMetaNodes()->getRootNodeNumID();
      rootIsBuddyMirrored = app->getMetaNodes()->getRootIsBuddyMirrored();
   }
   // reference/retrieve all nodes from the given store and send them

   auto nodeList = nodes->referenceAllNodes();

   ctx.sendResponse(GetNodesRespMsg(rootNumID, rootIsBuddyMirrored, nodeList));

   return true;
}

