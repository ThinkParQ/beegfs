#include <common/net/message/storage/mirroring/StorageResyncStartedRespMsg.h>
#include <common/toolkit/MessagingTk.h>

#include <program/Program.h>

#include "StorageResyncStartedMsgEx.h"

bool StorageResyncStartedMsgEx::processIncoming(ResponseContext& ctx)
{
   uint16_t targetID = getValue();

   deleteMirrorSessions(targetID);

   ctx.sendResponse(StorageResyncStartedRespMsg() );

   return true;
}

void StorageResyncStartedMsgEx::deleteMirrorSessions(uint16_t targetID)
{
   SessionStore* sessions = Program::getApp()->getSessions();
   NumNodeIDList sessionIDs;
   sessions->getAllSessionIDs(&sessionIDs);

   for (NumNodeIDListCIter iter = sessionIDs.begin(); iter != sessionIDs.end(); iter++)
   {
      auto session = sessions->referenceSession(*iter);

      if (!session) // meanwhile deleted
         continue;

      SessionLocalFileStore* sessionLocalFiles = session->getLocalFiles();
      sessionLocalFiles->removeAllMirrorSessions(targetID);
   }
}
