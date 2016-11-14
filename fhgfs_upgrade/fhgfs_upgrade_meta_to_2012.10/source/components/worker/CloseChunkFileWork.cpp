#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/session/opening/CloseChunkFileMsg.h>
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <program/Program.h>
#include "CloseChunkFileWork.h"


void CloseChunkFileWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

FhgfsOpsErr CloseChunkFileWork::communicate()
{
   const char* logContext = "Close chunk file work";

   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   NodeStore* nodes = Program::getApp()->getStorageNodes();

   CloseChunkFileMsg closeMsg(sessionID, fileHandleID, targetID);

   if(isMirror)
      closeMsg.setFlags(CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS | CLOSECHUNKFILEMSG_FLAG_ISMIRROR);

   Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
   if(!node)
   {
      LogContext(logContext).log(Log_WARNING,
         "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) + "; "
         "Session: " + std::string(sessionID) + "; "
         "FileHandle: " + fileHandleID);
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   // send request to node and receive response
   char* respBuf;
   NetMessage* respMsg;
   bool requestRes = MessagingTk::requestResponse(
      node, &closeMsg, NETMSGTYPE_CloseChunkFileResp, &respBuf, &respMsg);

   if(!requestRes)
   { // communication error
      LogContext(logContext).log(Log_WARNING,
         "Communication with storage node failed: " + node->getTypedNodeID() + "; "
         "Session: " + std::string(sessionID) + "; "
         "FileHandle: " + fileHandleID);

      nodes->releaseNode(&node);
      return FhgfsOpsErr_COMMUNICATION;
   }

   // correct response type received
   CloseChunkFileRespMsg* closeRespMsg = (CloseChunkFileRespMsg*)respMsg;

   FhgfsOpsErr closeRemoteRes = closeRespMsg->getResult();

   // set current dynamic attribs (even if result not success, because then storageVersion==0)
   if(outDynAttribs)
   {
      DynamicFileAttribs currentDynAttribs(closeRespMsg->getStorageVersion(),
         closeRespMsg->getFileSize(), closeRespMsg->getModificationTimeSecs(),
         closeRespMsg->getLastAccessTimeSecs() );

      *outDynAttribs = currentDynAttribs;
   }

   if(closeRemoteRes != FhgfsOpsErr_SUCCESS)
   { // error: local file not closed
      LogContext(logContext).log(Log_WARNING,
         "Storage node was unable to close chunk file: " + node->getTypedNodeID() + "; "
         "Error: " + FhgfsOpsErrTk::toErrString(closeRemoteRes) + "; "
         "Session: " + std::string(sessionID) + "; "
         "FileHandle: " + std::string(fileHandleID) );

      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);

      return closeRemoteRes;
   }

   // success: chunk file closed
   LOG_DEBUG(logContext, Log_DEBUG,
      "Storage node closed chunk file: " + node->getTypedNodeID() + "; "
      "Session: " + std::string(sessionID) + "; "
      "FileHandle: " + fileHandleID);

   nodes->releaseNode(&node);
   delete(respMsg);
   free(respBuf);

   return FhgfsOpsErr_SUCCESS;
}
