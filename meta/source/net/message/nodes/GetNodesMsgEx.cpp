#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <program/Program.h>
#include "GetNodesMsgEx.h"

#include <boost/lexical_cast.hpp>

bool GetNodesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetNodes incoming");

   NodeType nodeType = (NodeType)getValue();

   LOG_DEBUG_CONTEXT(log, 5, "NodeType: " + boost::lexical_cast<std::string>(nodeType));

   App* app = Program::getApp();

   NodeStore* nodes = app->getServerStoreFromType(nodeType);
   if(unlikely(!nodes) )
   {
      LOG(GENERAL, ERR, "Invalid node type.",
            ("Node Type", getNodeType()),
            ("Sender", ctx.peerName())
         );

      return true;
   }

   auto nodeList = nodes->referenceAllNodes();

   NumNodeID rootID = app->getMetaRoot().getID();
   bool rootIsBuddyMirrored = app->getMetaRoot().getIsMirrored();

   ctx.sendResponse(GetNodesRespMsg(rootID, rootIsBuddyMirrored, nodeList) );

   return true;
}

