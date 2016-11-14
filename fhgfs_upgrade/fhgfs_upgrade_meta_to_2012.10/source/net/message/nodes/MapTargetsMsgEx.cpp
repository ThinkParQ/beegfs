#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/net/msghelpers/MsgHelperAck.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "MapTargetsMsgEx.h"


bool MapTargetsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("MapTargetsMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, Log_DEBUG, std::string("Received a MapTargetsMsg from: ") + peer);

   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();

   uint16_t nodeID = getNodeID();
   UInt16List targetIDs;

   parseTargetIDs(&targetIDs);

   for(UInt16ListConstIter iter = targetIDs.begin(); iter != targetIDs.end(); iter++)
   {
      bool wasNewTarget = targetMapper->mapTarget(*iter, nodeID);
      if(wasNewTarget)
      {
         LOG_DEBUG_CONTEXT(log, Log_WARNING, "Mapping "
            "target " + StringTk::uintToStr(*iter) +
            " => " +
            storageNodes->getNodeIDWithTypeStr(nodeID) );

         IGNORE_UNUSED_VARIABLE(storageNodes);
      }
   }


   // send response

   if(!MsgHelperAck::respondToAckRequest(this, fromAddr, sock,
      respBuf, bufLen, app->getDatagramListener() ) )
   {
      MapTargetsRespMsg respMsg(FhgfsOpsErr_SUCCESS);
      respMsg.serialize(respBuf, bufLen);

      if(fromAddr)
      { // datagram => sync via dgramLis send method
         app->getDatagramListener()->sendto(respBuf, respMsg.getMsgLength(), 0,
            (struct sockaddr*)fromAddr, sizeof(*fromAddr) );
      }
      else
         sock->sendto(respBuf, respMsg.getMsgLength(), 0, NULL, 0);
   }

   return true;
}

