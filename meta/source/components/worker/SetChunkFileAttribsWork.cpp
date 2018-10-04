#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/storage/attribs/SetLocalAttrMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <components/worker/SetChunkFileAttribsWork.h>
#include <program/Program.h>


void SetChunkFileAttribsWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

FhgfsOpsErr SetChunkFileAttribsWork::communicate()
{
   const char* logContext = "Set chunk file attribs work";

   App* app = Program::getApp();

   SetLocalAttrMsg setAttrMsg(entryID, targetID, pathInfo, validAttribs, attribs, enableCreation);

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
   {
      setAttrMsg.addMsgHeaderFeatureFlag(SETLOCALATTRMSG_FLAG_BUDDYMIRROR);

      if(useBuddyMirrorSecond)
         setAttrMsg.addMsgHeaderFeatureFlag(SETLOCALATTRMSG_FLAG_BUDDYMIRROR_SECOND);
   }

   if(quotaChown)
      setAttrMsg.addMsgHeaderFeatureFlag(SETLOCALATTRMSG_FLAG_USE_QUOTA);

   setAttrMsg.setMsgHeaderUserID(msgUserID);

   // prepare communication

   RequestResponseTarget rrTarget(targetID, app->getTargetMapper(), app->getStorageNodes() );

   rrTarget.setTargetStates(app->getTargetStateStore() );

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
      rrTarget.setMirrorInfo(app->getStorageBuddyGroupMapper(), useBuddyMirrorSecond);

   RequestResponseArgs rrArgs(NULL, &setAttrMsg, NETMSGTYPE_SetLocalAttrResp);

   // communicate

   FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // communication error
      LogContext(logContext).log(Log_WARNING,
         "Communication with storage target failed. " +
         std::string(pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "entryID: " + entryID);

      return requestRes;
   }

   // correct response type received
   const auto setRespMsg = (const SetLocalAttrRespMsg*)rrArgs.outRespMsg.get();

   FhgfsOpsErr setRespVal = setRespMsg->getResult();
   if(setRespVal != FhgfsOpsErr_SUCCESS)
   { // error occurred
      LogContext(logContext).log(Log_WARNING,
         "Setting chunk file attributes on target failed. " +
         std::string(pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "EntryID: " + entryID);

      return setRespVal;
   }

   // success
   LOG_DEBUG(logContext, Log_DEBUG,
      "Set attribs of chunk file on target. " +
      std::string( (pattern->getPatternType() == StripePatternType_BuddyMirror) ? "Mirror " : "") +
      "TargetID: " + StringTk::uintToStr(targetID) + "; "
      "EntryID: " + entryID);

   if ((outDynamicAttribs)
      && (setRespMsg->isMsgHeaderFeatureFlagSet(SETLOCALATTRRESPMSG_FLAG_HAS_ATTRS)))
   {
      setRespMsg->getDynamicAttribs(outDynamicAttribs);
   }

   return FhgfsOpsErr_SUCCESS;
}

