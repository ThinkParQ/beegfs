#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/session/opening/CloseChunkFileMsg.h>
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <program/Program.h>
#include "CloseChunkFileWork.h"

#include <boost/lexical_cast.hpp>


void CloseChunkFileWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

FhgfsOpsErr CloseChunkFileWork::communicate()
{
   const char* logContext = "Close chunk file work";

   App* app = Program::getApp();

   // prepare request message

   CloseChunkFileMsg closeMsg(sessionID, fileHandleID, targetID, pathInfoPtr);

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
   {
      closeMsg.addMsgHeaderFeatureFlag(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR);

      if(useBuddyMirrorSecond)
      {
         closeMsg.addMsgHeaderFeatureFlag(CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS);
         closeMsg.addMsgHeaderFeatureFlag(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND);
      }
   }

   closeMsg.setMsgHeaderUserID(msgUserID);

   // prepare communication

   RequestResponseTarget rrTarget(targetID, app->getTargetMapper(), app->getStorageNodes() );

   rrTarget.setTargetStates(app->getTargetStateStore() );

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
      rrTarget.setMirrorInfo(app->getStorageBuddyGroupMapper(), useBuddyMirrorSecond);

   RequestResponseArgs rrArgs(NULL, &closeMsg, NETMSGTYPE_CloseChunkFileResp);

   // communicate

   FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   {
      LogContext(logContext).log(Log_WARNING,
         "Communication with storage target failed. " +
         std::string(pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "Session: " + sessionID.str() + "; "
         "FileHandle: " + fileHandleID);

      return requestRes;
   }

   // correct response type received
   CloseChunkFileRespMsg* closeRespMsg = (CloseChunkFileRespMsg*)rrArgs.outRespMsg.get();

   FhgfsOpsErr closeRemoteRes = closeRespMsg->getResult();

   // set current dynamic attribs (even if result not success, because then storageVersion==0)
   if(outDynAttribs)
   {
      DynamicFileAttribs currentDynAttribs(closeRespMsg->getStorageVersion(),
         closeRespMsg->getFileSize(), closeRespMsg->getAllocedBlocks(),
         closeRespMsg->getModificationTimeSecs(), closeRespMsg->getLastAccessTimeSecs() );

      *outDynAttribs = currentDynAttribs;
   }

   if(closeRemoteRes != FhgfsOpsErr_SUCCESS)
   { // error: chunk file not closed
      int logLevel = Log_WARNING;

      if(closeRemoteRes == FhgfsOpsErr_INUSE)
         logLevel = Log_DEBUG; // happens on ctrl+c, so don't irritate user with these log msgs

      LogContext(logContext).log(logLevel,
         "Closing chunk file on target failed. " +
         std::string(pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "Error: " + boost::lexical_cast<std::string>(closeRemoteRes) + "; "
         "Session: " + sessionID.str() + "; "
         "FileHandle: " + std::string(fileHandleID) );

      return closeRemoteRes;
   }

   // success: chunk file closed

   LOG_DEBUG(logContext, Log_DEBUG,
      "Closed chunk file on target. " +
      std::string( (pattern->getPatternType() == StripePatternType_BuddyMirror) ? "Mirror " : "") +
      "TargetID: " + StringTk::uintToStr(targetID) + "; "
      "Session: " + sessionID.str() + "; "
      "FileHandle: " + fileHandleID);

   return FhgfsOpsErr_SUCCESS;
}
