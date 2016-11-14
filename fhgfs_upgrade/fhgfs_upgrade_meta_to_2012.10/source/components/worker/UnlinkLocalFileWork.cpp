#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <program/Program.h>
#include "UnlinkLocalFileWork.h"


void UnlinkLocalFileWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

FhgfsOpsErr UnlinkLocalFileWork::communicate()
{
   const char* logContext = "Unlink chunk file work";

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStore* nodes = app->getStorageNodes();

   UnlinkLocalFileMsg unlinkMsg(entryID, targetID);
   unlinkMsg.setMirroredFromTargetID(mirroredFromTargetID);

   Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
   if(unlikely(!node) )
   {
      // note: not an error, because we don't care about removed nodes
      LogContext(logContext).log(Log_WARNING,
         "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) + "; "
         "entryID: " + entryID);
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   // send request to node and receive response
   char* respBuf;
   NetMessage* respMsg;
   bool requestRes = MessagingTk::requestResponse(
      node, &unlinkMsg, NETMSGTYPE_UnlinkLocalFileResp, &respBuf, &respMsg);

   if(unlikely(!requestRes) )
   { // communication error
      LogContext(logContext).log(Log_WARNING,
         "Communication with storage node failed: " + node->getTypedNodeID() + "; "
         "entryID: " + entryID);

      nodes->releaseNode(&node);
      return FhgfsOpsErr_COMMUNICATION;
   }

   // correct response type received
   UnlinkLocalFileRespMsg* unlinkRespMsg = (UnlinkLocalFileRespMsg*)respMsg;

   bool unlinkSuccess = (unlinkRespMsg->getValue() == 0);
   if(!unlinkSuccess)
   { // error: local file not unlinked
      LogContext(logContext).log(Log_WARNING,
         "Storage node was unable to unlink chunk file: " + node->getTypedNodeID() + "; "
         "entryID: " + entryID);

      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
      return FhgfsOpsErr_INTERNAL;
   }

   // success: local file unlinked
   LOG_DEBUG(logContext, Log_DEBUG,
      "Storage node unlinked chunk file: " + node->getTypedNodeID() + "; "
      "entryID: " + entryID);

   nodes->releaseNode(&node);
   delete(respMsg);
   free(respBuf);

   return FhgfsOpsErr_SUCCESS;
}

