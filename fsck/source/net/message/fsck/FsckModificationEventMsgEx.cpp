#include <program/Program.h>

#include "FsckModificationEventMsgEx.h"

bool FsckModificationEventMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("FsckModificationEventMsg incoming");

   App* app = Program::getApp();
   ModificationEventHandler *eventHandler = app->getModificationEventHandler();

   StringList& entryIDList = this->getEntryIDList();

   bool addRes = eventHandler->add(getModificationEventTypeList(), entryIDList);
   if (!addRes)
   {
      log.logErr("Unable to add modification event to database");
      return false;
   }

   bool ackRes = acknowledge(ctx);
   if (!ackRes)
      log.logErr("Unable to send ack to metadata server");

   return true;
}
