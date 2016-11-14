#include <common/net/message/session/locking/FLockRangeRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SessionTk.h>
#include <net/msghelpers/MsgHelperLocking.h>
#include <program/Program.h>
#include <session/SessionStore.h>
#include <storage/MetaStore.h>
#include "FLockRangeMsgEx.h"


bool FLockRangeMsgEx::processIncoming(ResponseContext& ctx)
{
   /* note: this code is very similar to FLockEntryMsgEx::processIncoming(), so if you change
      something here, you probably want to change it there, too. */

#ifdef BEEGFS_DEBUG
   const char* logContext = "FLockRangeMsg incoming";

   LOG_DEBUG(logContext, Log_DEBUG, "Received a FLockRangeMsg from: " + ctx.peerName() );
#endif // BEEGFS_DEBUG

   clientResult = FhgfsOpsErr_INTERNAL;

   Program::getApp()->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
         MetaOpCounter_FLOCKRANGE, getMsgHeaderUserID());

   return BaseType::processIncoming(ctx);
}

FileIDLock FLockRangeMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID()};
}

std::unique_ptr<MirroredMessageResponseState> FLockRangeMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   unsigned ownerFD = SessionTk::ownerFDFromHandleID(getFileHandleID() );

   RangeLockDetails lockDetails(getClientNumID(), getOwnerPID(), getLockAckID(),
      getLockTypeFlags(), getStart(), getEnd() );

   LOG_DBG(SPAM, lockDetails.toString());

   EntryInfo* entryInfo = getEntryInfo();

   SessionStore* sessions = entryInfo->getIsBuddyMirrored()
      ? app->getMirroredSessions()
      : app->getSessions();

   // find sessionFile
   Session* session = sessions->referenceSession(getClientNumID(), true);
   SessionFileStore* sessionFiles = session->getFiles();

   SessionFile* sessionFile = sessionFiles->referenceSession(ownerFD);
   if(!sessionFile)
   { // sessionFile not exists (mds restarted?)

      // check if this is just an UNLOCK REQUEST

      if(getLockTypeFlags() & ENTRYLOCKTYPE_LOCKOPS_REMOVE)
      { // it's an unlock => we'll just ignore it (since the locks are gone anyways)
         clientResult = FhgfsOpsErr_SUCCESS;

         goto cleanup_session;
      }

      // check if this is a LOCK CANCEL REQUEST

      if(getLockTypeFlags() & ENTRYLOCKTYPE_CANCEL)
      { // it's a lock cancel
         /* this is an important special case, because client might have succeeded in closing the
         file but the conn might have been interrupted during unlock, so we definitely have to try
         canceling the lock here */

         // if the file still exists, just do the lock cancel without session recovery attempt

         MetaFileHandle lockCancelFile = metaStore->referenceFile(entryInfo);
         if(lockCancelFile)
         {
            lockCancelFile->flockRange(lockDetails);
            metaStore->releaseFile(entryInfo->getParentEntryID(), lockCancelFile);
         }

         clientResult = FhgfsOpsErr_SUCCESS;

         goto cleanup_session;
      }

      // it's a LOCK REQUEST => try to recover session file to do the locking

      clientResult = MsgHelperLocking::trySesssionRecovery(entryInfo, getClientNumID(),
         ownerFD, sessionFiles, &sessionFile);

      // (note: sessionFile==NULL now if recovery was not successful)

   } // end of session file session recovery attempt

   if(sessionFile)
   { // sessionFile exists (or was successfully recovered)
      auto& file = sessionFile->getInode();

      auto lockGranted = file->flockRange(lockDetails);
      if (!lockGranted.first)
         clientResult = FhgfsOpsErr_WOULDBLOCK;
      else
         clientResult = FhgfsOpsErr_SUCCESS;

      if ((getLockTypeFlags() & ENTRYLOCKTYPE_UNLOCK) && !file->incrementFileVersion(entryInfo))
      {
         LOG(ERR, "Could not bump file version.", file->getEntryID());
         sessionFiles->releaseSession(sessionFile, entryInfo);
         clientResult = FhgfsOpsErr_INTERNAL;
         goto cleanup_session;
      }

      if (!lockGranted.second.empty() && !isSecondary)
         LockingNotifier::notifyWaitersRangeLock(file->getReferenceParentID(), file->getEntryID(),
               file->getIsBuddyMirrored(), std::move(lockGranted.second));

      // cleanup
      sessionFiles->releaseSession(sessionFile, entryInfo);
   }


   // cleanup
cleanup_session:
   sessions->releaseSession(session);

   LOG_DBG(SPAM, "", clientResult);

   return boost::make_unique<ResponseState>(clientResult);
}

bool FLockRangeMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   return sendToSecondary(ctx, *this, NETMSGTYPE_FLockRangeResp, clientResult)
      == FhgfsOpsErr_SUCCESS;
}
