#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "GetTargetMappingsMsgEx.h"


bool GetTargetMappingsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("GetTargetMappingsMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a GetTargetMappingsMsg from: ") + peer);

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();

   UInt16List targetIDs;
   UInt16List nodeIDs;

   targetMapper->getMappingAsLists(targetIDs, nodeIDs);


   GetTargetMappingsRespMsg respMsg(&targetIDs, &nodeIDs);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, NULL, 0);

   return true;
}

