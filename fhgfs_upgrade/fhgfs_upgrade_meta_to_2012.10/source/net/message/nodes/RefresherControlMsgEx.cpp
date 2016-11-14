#include <common/net/message/nodes/RefresherControlRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RefresherControlMsgEx.h"


bool RefresherControlMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "RefresherControlMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a RefresherControlMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   App* app = Program::getApp();
   FullRefresher* refresher = app->getFullRefresher();

   RefresherControlType controlCommand = getControlCommand();

   switch(controlCommand)
   {
      case RefresherControl_START:
      {
         refresher->startRefreshing();
      } break;

      case RefresherControl_STOP:
      {
         refresher->stopRefreshing();
      } break;

      default:
      { // query status
         // nothing special to be done here, as we always respond with the current stats
      } break;

   }

   // query status

   bool isRunning = refresher->getIsRunning();

   uint64_t numDirsRefreshed;
   uint64_t numFilesRefreshed;

   refresher->getNumRefreshed(&numDirsRefreshed, &numFilesRefreshed);

   // send response

   RefresherControlRespMsg respMsg(isRunning, numDirsRefreshed, numFilesRefreshed);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   return true;
}

