#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/nodes/TargetCapacityPools.h>
#include <program/Program.h>
#include "GetNodeCapacityPoolsMsgEx.h"

bool GetNodeCapacityPoolsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("GetNodeCapacityPools incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a GetNodeCapacityPoolsMsg from: " + peer);

   NodeType nodeType = (NodeType)getValue();

   LOG_DEBUG_CONTEXT(log, Log_SPAM, "NodeType: " + Node::nodeTypeToStr(nodeType) );

   App* app = Program::getApp();
   TargetCapacityPools* pools = NULL;


   switch(nodeType)
   {
      case NODETYPE_Meta:
         pools = app->getMetaCapacityPools(); break;

      case NODETYPE_Storage:
         pools = app->getStorageCapacityPools(); break;

      default:
      {
         log.logErr("Invalid node type: " + StringTk::intToStr(nodeType) +
            "(" + Node::nodeTypeToStr(nodeType) + ")");

         return false;
      } break;
   }


   UInt16List listNormal;
   UInt16List listLow;
   UInt16List listEmergency;

   pools->getPoolsAsLists(listNormal, listLow, listEmergency);

   GetNodeCapacityPoolsRespMsg respMsg(&listNormal, &listLow, &listEmergency);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );


   return true;
}

