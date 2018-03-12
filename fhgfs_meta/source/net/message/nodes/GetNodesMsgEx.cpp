#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <program/Program.h>
#include "GetNodesMsgEx.h"

bool GetNodesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetNodes incoming");

   LOG_DEBUG_CONTEXT(log, 4, "Received a GetNodesMsg from: " + ctx.peerName() );

   NodeType nodeType = (NodeType)getValue();

   LOG_DEBUG_CONTEXT(log, 5, std::string("NodeType: ") + Node::nodeTypeToStr(nodeType) );

   App* app = Program::getApp();

   NodeStore* nodes = app->getServerStoreFromType(nodeType);
   if(unlikely(!nodes) )
   {
      LOG(ERR, "Invalid node type.",
            as("Node Type", Node::nodeTypeToStr(getNodeType())),
            as("Sender", ctx.peerName())
         );

      return true;
   }

   auto nodeList = nodes->referenceAllNodes();

   NumNodeID rootID = nodes->getRootNodeNumID();
   bool rootIsBuddyMirrored = nodes->getRootIsBuddyMirrored();

   ctx.sendResponse(GetNodesRespMsg(rootID, rootIsBuddyMirrored, nodeList) );

   return true;
}

