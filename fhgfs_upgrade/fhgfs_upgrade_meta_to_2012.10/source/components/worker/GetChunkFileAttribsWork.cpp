#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsMsg.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <program/Program.h>
#include "GetChunkFileAttribsWork.h"

void GetChunkFileAttribsWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

/**
 * @return true if communication successful
 */
FhgfsOpsErr GetChunkFileAttribsWork::communicate()
{
   const char* logContext = "Stat chunk file work";

   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   NodeStore* nodes = Program::getApp()->getStorageNodes();

   GetChunkFileAttribsMsg getSizeMsg(entryID, targetID);

   Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
   if(!node)
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
      node, &getSizeMsg, NETMSGTYPE_GetChunkFileAttribsResp, &respBuf, &respMsg);

   if(!requestRes)
   { // communication error
      LogContext(logContext).log(Log_WARNING,
         "Communication with node failed: " + node->getTypedNodeID() + "; "
         "entryID: " + entryID);

      nodes->releaseNode(&node);
      return FhgfsOpsErr_COMMUNICATION;
   }

   // correct response type received
   GetChunkFileAttribsRespMsg* getSizeRespMsg = (GetChunkFileAttribsRespMsg*)respMsg;

   bool getSizeSuccess = (getSizeRespMsg->getResult() == 0);
   if(!getSizeSuccess)
   { // error: local file not unlinked
      LogContext(logContext).log(Log_WARNING,
         "Node was unable to get local filesize: " + node->getTypedNodeID() + "; "
         "entryID: " + entryID);

      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
      return FhgfsOpsErr_INTERNAL;
   }

   // success: local file size refreshed

   DynamicFileAttribs currentDynAttribs(getSizeRespMsg->getStorageVersion(),
      getSizeRespMsg->getSize(), getSizeRespMsg->getModificationTimeSecs(),
      getSizeRespMsg->getLastAccessTimeSecs() );

   *outDynAttribs = currentDynAttribs;

   nodes->releaseNode(&node);
   delete(respMsg);
   free(respBuf);

   return FhgfsOpsErr_SUCCESS;
}
