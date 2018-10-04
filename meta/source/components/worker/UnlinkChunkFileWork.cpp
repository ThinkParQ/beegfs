#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <components/worker/UnlinkChunkFileWork.h>
#include <program/Program.h>


void UnlinkChunkFileWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

FhgfsOpsErr UnlinkChunkFileWork::communicate()
{
   const char* logContext = "Unlink chunk file work";

   App* app = Program::getApp();

   UnlinkLocalFileMsg unlinkMsg(entryID, targetID, pathInfo);

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
   {
      unlinkMsg.addMsgHeaderFeatureFlag(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR);

      if(useBuddyMirrorSecond)
         unlinkMsg.addMsgHeaderFeatureFlag(UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);
   }

   unlinkMsg.setMsgHeaderUserID(msgUserID);

   // prepare communication

   RequestResponseTarget rrTarget(targetID, app->getTargetMapper(), app->getStorageNodes() );

   rrTarget.setTargetStates(app->getTargetStateStore() );

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
      rrTarget.setMirrorInfo(app->getStorageBuddyGroupMapper(), useBuddyMirrorSecond);

   RequestResponseArgs rrArgs(NULL, &unlinkMsg, NETMSGTYPE_UnlinkLocalFileResp);

   // communicate

   FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // communication error
      LogContext(logContext).log(Log_WARNING,
         "Communication with storage target failed. " +
         std::string( (pattern->getPatternType() == StripePatternType_BuddyMirror) ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "EntryID: " + entryID);

      return requestRes;
   }

   // correct response type received
   UnlinkLocalFileRespMsg* unlinkRespMsg = (UnlinkLocalFileRespMsg*)rrArgs.outRespMsg.get();

   FhgfsOpsErr unlinkResult = unlinkRespMsg->getResult();
   if(unlinkResult != FhgfsOpsErr_SUCCESS)
   { // error: local file not unlinked
      LogContext(logContext).log(Log_WARNING,
         "Unlinking of chunk file from target failed. " +
         std::string(pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "EntryID: " + entryID);

      return unlinkResult;
   }

   // success: chunk file unlinked
   LOG_DEBUG(logContext, Log_DEBUG,
      "Chunk file unlinked from target. " +
      std::string( (pattern->getPatternType() == StripePatternType_BuddyMirror) ? "Mirror " : "") +
      "TargetID: " + StringTk::uintToStr(targetID) + "; "
      "EntryID: " + entryID);

   return FhgfsOpsErr_SUCCESS;
}

