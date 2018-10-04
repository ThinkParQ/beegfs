#include <program/Program.h>
#include <common/net/message/helperd/LogRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include "LogMsgEx.h"


bool LogMsgEx::processIncoming(ResponseContext& ctx)
{
   int logRes = 0;

   // Note: level 0 is reserved for error logging (logErr() )


   int logLevel = getLevel();
   Logger* logger = Logger::getLogger();
   const char* threadName = getThreadName();
   const char* context = getContext();
   const char* logMsg = getLogMsg();

   // note: we prepend an asterisk to remote threadNames, so that the user can see the difference
   std::string remoteThreadName = std::string("*") + threadName +
      "(" + StringTk::intToStr(getThreadID() ) + ")";

   if(logLevel)
   { // log standard message
      logger->logForcedWithThreadName(logLevel, remoteThreadName.c_str(), context, logMsg);
   }
   else
   { // log error message
      logger->logErrWithThreadName(remoteThreadName.c_str(), context, logMsg);
   }

   ctx.sendResponse(LogRespMsg(logRes) );

   return true;
}
