#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/storage/TruncLocalFileMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <components/worker/TruncChunkFileWork.h>
#include <program/Program.h>

#include <boost/lexical_cast.hpp>


void TruncChunkFileWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate();
   *outResult = commRes;

   counter->incCount();
}

FhgfsOpsErr TruncChunkFileWork::communicate()
{
   const char* logContext = "Trunc chunk file work";

   App* app = Program::getApp();

   TruncLocalFileMsg truncMsg(filesize, entryID, targetID, pathInfo);

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
   {
      truncMsg.addMsgHeaderFeatureFlag(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR);

      if(useBuddyMirrorSecond)
      {
         truncMsg.addMsgHeaderFeatureFlag(TRUNCLOCALFILEMSG_FLAG_NODYNAMICATTRIBS);
         truncMsg.addMsgHeaderFeatureFlag(TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);
      }
   }

   if(useQuota)
      truncMsg.setUserdataForQuota(userID, groupID);

   truncMsg.setMsgHeaderUserID(msgUserID);

   // prepare communication

   RequestResponseTarget rrTarget(targetID, app->getTargetMapper(), app->getStorageNodes() );

   rrTarget.setTargetStates(app->getTargetStateStore() );

   if(pattern->getPatternType() == StripePatternType_BuddyMirror)
      rrTarget.setMirrorInfo(app->getStorageBuddyGroupMapper(), useBuddyMirrorSecond);

   RequestResponseArgs rrArgs(NULL, &truncMsg, NETMSGTYPE_TruncLocalFileResp);

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
   TruncLocalFileRespMsg* truncRespMsg = (TruncLocalFileRespMsg*)rrArgs.outRespMsg.get();

   FhgfsOpsErr truncRespVal = truncRespMsg->getResult();

   // set current dynamic attribs (even if result not success, because then storageVersion==0)
   if(outDynAttribs)
   {
      DynamicFileAttribs currentDynAttribs(truncRespMsg->getStorageVersion(),
         truncRespMsg->getFileSize(), truncRespMsg->getAllocedBlocks(),
         truncRespMsg->getModificationTimeSecs(), truncRespMsg->getLastAccessTimeSecs() );

      *outDynAttribs = currentDynAttribs;
   }

   if(unlikely(truncRespVal != FhgfsOpsErr_SUCCESS) )
   {  // error: chunk file not truncated
      if(truncRespVal == FhgfsOpsErr_TOOBIG)
         return truncRespVal; // will be passed through to user app on client, so don't log here

      LogContext(logContext).log(Log_WARNING,
         "Truncation of chunk file on target failed. " +
         std::string(pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "EntryID: " + entryID + "; "
         "Error: " + boost::lexical_cast<std::string>(truncRespVal) );

      return truncRespVal;
   }

   // success: chunk file truncated

   LOG_DEBUG(logContext, Log_DEBUG,
      "Chunk file truncated on target. " +
      std::string( (pattern->getPatternType() == StripePatternType_BuddyMirror) ? "Mirror " : "") +
      "TargetID: " + StringTk::uintToStr(targetID) + "; "
      "EntryID: " + entryID);


   return FhgfsOpsErr_SUCCESS;
}
