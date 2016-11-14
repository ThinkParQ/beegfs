#include <common/net/message/storage/mirroring/StorageResyncStartedRespMsg.h>
#include <common/toolkit/MessagingTk.h>

#include <program/Program.h>

#include "StorageResyncStartedMsgEx.h"

bool StorageResyncStartedMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "StorageResyncStartedMsg incoming";
      LOG_DEBUG(logContext, Log_DEBUG,
         "Received a StorageResyncStartedMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

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
      Session* session = sessions->referenceSession(*iter, false);

      if (!session) // meanwhile deleted
         continue;

      SessionLocalFileStore* sessionLocalFiles = session->getLocalFiles();
      sessionLocalFiles->removeAllMirrorSessions(targetID);
   }
}
