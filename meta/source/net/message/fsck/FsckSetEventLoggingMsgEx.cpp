#include <components/ModificationEventFlusher.h>
#include <program/Program.h>
#include "FsckSetEventLoggingMsgEx.h"

bool FsckSetEventLoggingMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("FsckSetEventLoggingMsg incoming");

   App* app = Program::getApp();
   ModificationEventFlusher* flusher = app->getModificationEventFlusher();

   bool result;
   bool loggingEnabled;
   bool missedEvents;

   bool enableLogging = this->getEnableLogging();

   if (enableLogging)
   {
      loggingEnabled = flusher->enableLogging(getPortUDP(), getNicList(), getForceRestart());
      result = true; // (always true when logging is enabled)
      missedEvents = true; // (value ignored when logging is enabled)
   }
   else
   { // disable logging
      result = flusher->disableLogging();
      loggingEnabled = false; // (value ignored when logging is disabled)
      missedEvents = flusher->getFsckMissedEvent();
   }

   ctx.sendResponse(FsckSetEventLoggingRespMsg(result, loggingEnabled, missedEvents));

   return true;
}
