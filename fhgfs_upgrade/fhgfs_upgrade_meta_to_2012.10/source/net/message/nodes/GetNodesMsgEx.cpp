#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <program/Program.h>
#include "GetNodesMsgEx.h"

bool GetNodesMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("GetNodes incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a GetNodesMsg from: ") + peer);

   NodeType nodeType = (NodeType)getValue();
   
   LOG_DEBUG_CONTEXT(log, 5, std::string("NodeType: ") + Node::nodeTypeToStr(nodeType) );
      
   App* app = Program::getApp();
   NodeList nodeList;

   NodeStore* nodes = app->getServerStoreFromType(nodeType);
   if(unlikely(!nodes) )
   {
      log.logErr("Invalid node type: " + StringTk::intToStr(nodeType) +
         "(" + Node::nodeTypeToStr(nodeType) + ")");

      return true;
   }

   nodes->referenceAllNodes(&nodeList);

   uint16_t rootID = nodes->getRootNodeNumID();

   
   GetNodesRespMsg respMsg(rootID, &nodeList);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );
   
   
   nodes->releaseAllNodes(&nodeList);
   
   return true;
}

