#include <common/net/msghelpers/MsgHelperAck.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RefreshCapacityPoolsMsgEx.h"


bool RefreshCapacityPoolsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("RefreshCapacityPoolsMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a RefreshCapacityPoolsMsg from: ") + peer);

   App* app = Program::getApp();
   InternodeSyncer* syncer = app->getInternodeSyncer();


   // force update of capacity pools
   syncer->setForcePoolsUpdate();


   // send response

   MsgHelperAck::respondToAckRequest(this, fromAddr, sock, respBuf, bufLen,
      app->getDatagramListener() );

   return true;
}

