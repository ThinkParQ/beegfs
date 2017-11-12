#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/storage/attribs/SetLocalAttrMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <program/Program.h>
#include "SetLocalFileAttribsWork.h"


void SetLocalFileAttribsWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

FhgfsOpsErr SetLocalFileAttribsWork::communicate()
{
   const char* logContext = "Set chunk file attribs work";

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStore* nodes = app->getStorageNodes();

   SetLocalAttrMsg setAttrMsg(entryID, targetID, validAttribs, attribs, enableCreation);
   setAttrMsg.setMirroredFromTargetID(mirroredFromTargetID);

   Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
   if(unlikely(!node) )
   {
      // note: not an error, because we don't care about removed nodes
      LogContext(logContext).log(Log_WARNING, "Unable to resolve storage node targetID: " +
         StringTk::uintToStr(targetID) + "; "
         "entryID: " + entryID);
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   // send request to node and receive response
   char* respBuf;
   NetMessage* respMsg;
   bool requestRes = MessagingTk::requestResponse(
      node, &setAttrMsg, NETMSGTYPE_SetLocalAttrResp, &respBuf, &respMsg);

   if(unlikely(!requestRes) )
   { // communication error
      LogContext(logContext).log(Log_WARNING,
         "Communication with node failed: " + node->getTypedNodeID() + "; "
         "entryID: " + entryID);

      nodes->releaseNode(&node);
      return FhgfsOpsErr_COMMUNICATION;
   }

   // correct response type received
   SetLocalAttrRespMsg* setRespMsg = (SetLocalAttrRespMsg*)respMsg;

   FhgfsOpsErr setRespVal = (FhgfsOpsErr)setRespMsg->getValue();
   if(setRespVal != FhgfsOpsErr_SUCCESS)
   { // error occurred
      LogContext(logContext).log(Log_WARNING,
         "Node was unable to set attribs of chunk file: " + node->getTypedNodeID() + "; "
         "entryID: " + entryID);

      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
      return FhgfsOpsErr_INTERNAL;
   }

   // success: chunk file unlinked
   LOG_DEBUG(logContext, Log_DEBUG,
      "Node has set attribs of chunk file: " + node->getTypedNodeID() + "; "
      "entryID: " + entryID);

   nodes->releaseNode(&node);
   delete(respMsg);
   free(respBuf);

   return FhgfsOpsErr_SUCCESS;
}

