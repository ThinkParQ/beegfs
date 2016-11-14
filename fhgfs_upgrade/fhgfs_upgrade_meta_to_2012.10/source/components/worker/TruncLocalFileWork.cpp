#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/storage/TruncLocalFileMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <program/Program.h>
#include "TruncLocalFileWork.h"


void TruncLocalFileWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

FhgfsOpsErr TruncLocalFileWork::communicate()
{
   const char* logContext = "Trunc chunk file work";

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStore* nodes = app->getStorageNodes();

   TruncLocalFileMsg truncMsg(filesize, entryID, targetID);

   if(mirroredFromTargetID)
   {
      truncMsg.setFlags(TRUNCLOCALFILEMSG_FLAG_NODYNAMICATTRIBS);
      truncMsg.setMirroredFromTargetID(mirroredFromTargetID);
   }

   Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
   if(unlikely(!node) )
   {
      LogContext(logContext).log(Log_WARNING,
         "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) + "; "
         "entryID: " + entryID);
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   // send request to node and receive response
   char* respBuf;
   NetMessage* respMsg;
   bool requestRes = MessagingTk::requestResponse(
      node, &truncMsg, NETMSGTYPE_TruncLocalFileResp, &respBuf, &respMsg);

   if(unlikely(!requestRes) )
   { // communication error
      LogContext(logContext).log(Log_WARNING,
         "Communication with storage node failed: " + node->getTypedNodeID() + "; "
         "entryID: " + entryID);

      nodes->releaseNode(&node);
      return FhgfsOpsErr_COMMUNICATION;
   }

   // correct response type received
   TruncLocalFileRespMsg* truncRespMsg = (TruncLocalFileRespMsg*)respMsg;

   FhgfsOpsErr truncRespVal = (FhgfsOpsErr)truncRespMsg->getResult();

   // set current dynamic attribs (even if result not success, because then storageVersion==0)
   if(outDynAttribs)
   {
      DynamicFileAttribs currentDynAttribs(truncRespMsg->getStorageVersion(),
         truncRespMsg->getFileSize(), truncRespMsg->getModificationTimeSecs(),
         truncRespMsg->getLastAccessTimeSecs() );

      *outDynAttribs = currentDynAttribs;
   }

   if(truncRespVal != FhgfsOpsErr_SUCCESS)
   { // error: local file not truncated
      LogContext(logContext).log(Log_WARNING,
         "Storage node was unable to truncate chunk file: " + node->getTypedNodeID() + "; "
         "entryID: " + entryID);

      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
      return truncRespVal;
   }

   // success: local file truncated
   LOG_DEBUG(logContext, Log_DEBUG,
      "Storage node truncated chunk file: " + node->getTypedNodeID() + "; "
      "entryID: " + entryID);

   nodes->releaseNode(&node);
   delete(respMsg);
   free(respBuf);

   return FhgfsOpsErr_SUCCESS;
}
