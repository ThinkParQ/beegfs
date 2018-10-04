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

   App* app = Program::getApp();

   GetChunkFileAttribsMsg getSizeMsg(entryID, targetID, pathInfo);

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
   {
      getSizeMsg.addMsgHeaderFeatureFlag(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR);

      if(useBuddyMirrorSecond)
         getSizeMsg.addMsgHeaderFeatureFlag(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR_SECOND);
   }

   getSizeMsg.setMsgHeaderUserID(msgUserID);

   // prepare communication

   RequestResponseTarget rrTarget(targetID, app->getTargetMapper(), app->getStorageNodes() );

   rrTarget.setTargetStates(app->getTargetStateStore() );

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
      rrTarget.setMirrorInfo(app->getStorageBuddyGroupMapper(), useBuddyMirrorSecond);

   RequestResponseArgs rrArgs(NULL, &getSizeMsg, NETMSGTYPE_GetChunkFileAttribsResp);

   // communicate

   FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // communication error
      LogContext(logContext).log(Log_WARNING,
         "Communication with storage target failed. " +
         std::string(pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "EntryID: " + entryID);

      return requestRes;
   }

   // correct response type received
   auto* getSizeRespMsg = (GetChunkFileAttribsRespMsg*)rrArgs.outRespMsg.get();

   FhgfsOpsErr getSizeResult = getSizeRespMsg->getResult();
   if(getSizeResult != FhgfsOpsErr_SUCCESS)
   { // error: chunk file not unlinked
      LogContext(logContext).log(Log_WARNING,
         "Getting chunk file attributes from target failed. " +
         std::string(pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "EntryID: " + entryID);

      return getSizeResult;
   }

   // success: chunk file dynamic attribs refreshed

   DynamicFileAttribs currentDynAttribs(getSizeRespMsg->getStorageVersion(),
      getSizeRespMsg->getSize(), getSizeRespMsg->getAllocedBlocks(),
      getSizeRespMsg->getModificationTimeSecs(), getSizeRespMsg->getLastAccessTimeSecs() );

   *outDynAttribs = currentDynAttribs;

   return FhgfsOpsErr_SUCCESS;
}
