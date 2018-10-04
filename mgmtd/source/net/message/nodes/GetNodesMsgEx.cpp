#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <program/Program.h>
#include "GetNodesMsgEx.h"

#include <boost/lexical_cast.hpp>

bool GetNodesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetNodes incoming");

   App* app = Program::getApp();
   NodeType nodeType = getNodeType();

   LOG_DEBUG_CONTEXT(log, Log_SPAM, "NodeType: " + boost::lexical_cast<std::string>(nodeType));


   // get corresponding node store

   AbstractNodeStore* nodes = app->getAbstractNodeStoreFromType(nodeType);
   if(!nodes)
   {
      LOG(GENERAL, ERR, "Invalid node type.", nodeType, ("Sender", ctx.peerName()));
      return false;
   }

   // get root ID

   NumNodeID rootNumID;
   bool rootIsBuddyMirrored = false;

   if(nodeType == NODETYPE_Meta)
   {
      rootNumID = app->getMetaRoot().getID();
      rootIsBuddyMirrored = app->getMetaRoot().getIsMirrored();
   }
   // reference/retrieve all nodes from the given store and send them

   auto nodeList = nodes->referenceAllNodes();

   ctx.sendResponse(GetNodesRespMsg(rootNumID, rootIsBuddyMirrored, nodeList));

   return true;
}

