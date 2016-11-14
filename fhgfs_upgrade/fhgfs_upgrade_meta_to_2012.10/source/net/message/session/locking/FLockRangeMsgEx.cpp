#include <common/net/message/session/locking/FLockRangeRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SessionTk.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include "FLockRangeMsgEx.h"


bool FLockRangeMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   /* note: this code is very similar to FLockEntryMsgEx::processIncoming(), so if you change
      something here, you probably want to change it there, too. */

   const char* logContext = "FLockRangeMsg incoming";

   #ifdef FHGFS_DEBUG
      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a FLockRangeMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();
   MetaStore* metaStore = app->getMetaStore();

   FhgfsOpsErr clientResult = FhgfsOpsErr_INTERNAL; // reponse error code

   unsigned ownerFD = SessionTk::ownerFDFromHandleID(getFileHandleID() );

   RangeLockDetails lockDetails(getClientID(), getOwnerPID(), getLockAckID(),
      getLockTypeFlags(), getStart(), getEnd() );

   LOG_DEBUG(logContext, Log_SPAM, lockDetails.toString() );

   EntryInfo* entryInfo = this->getEntryInfo();

   // find sessionFile
   Session* session = sessions->referenceSession(getClientID(), true);
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

         FileInode* lockCancelFile = metaStore->referenceFile(entryInfo);
         if(lockCancelFile)
         {
            lockCancelFile->flockRange(lockDetails);
            metaStore->releaseFile(entryInfo->getParentEntryID(), lockCancelFile);
         }

         clientResult = FhgfsOpsErr_SUCCESS;

         goto cleanup_session;
      }

      // it's a LOCK REQUEST => try to recover session file to do the locking

      /* note: we re-open the file (in r+w mode, since we don't know the orig mode) because that's
         better than completely failing after mds restart and generally do what we can to let the
         caller go on as if nothing was wrong... */

      LogContext(logContext).log(Log_WARNING, std::string("Attempting recovery of file session ") +
         "(session: " + getClientID()                 + "; "
         "handle: "   + StringTk::uintToStr(ownerFD)  + "; "
         "parentID: " + entryInfo->getParentEntryID() + "; "
         "entryID: "  + entryInfo->getEntryID()       + ")" );

      FileInode* recoveryFile;
      unsigned recoveryAccessFlags = OPENFILE_ACCESS_READWRITE; /* (r+w is our only option, since we
         don't know the original flags) */

      FhgfsOpsErr openRes = metaStore->openFile(entryInfo, recoveryAccessFlags,
         &recoveryFile);
      if(openRes != FhgfsOpsErr_SUCCESS)
      { // file could not be opened => there's nothing we can do in this case
         LogContext(logContext).log(Log_WARNING, std::string("Recovery of file session failed: ") +
            FhgfsOpsErrTk::toErrString(openRes) );

         clientResult = openRes;
      }
      else
      { // file opened => try to insert the recovery file session
         SessionFile* recoverySessionFile = new SessionFile(recoveryFile, recoveryAccessFlags,
            entryInfo);
         recoverySessionFile->setSessionID(ownerFD);

         sessionFile = sessionFiles->addAndReferenceRecoverySession(recoverySessionFile);

         if(!sessionFile)
         { // bad, our recovery session ID was used by someone in the meantime => cleanup
            unsigned numHardlinks; // ignored here
            unsigned numInodeRefs; // ignored here

            LogContext(logContext).log(Log_WARNING, "Recovery of file session failed: "
               "SessionID is in use by another file now.");

            metaStore->closeFile(entryInfo, recoveryFile, recoveryAccessFlags,
               &numHardlinks, &numInodeRefs);

            delete(recoverySessionFile);

            clientResult = FhgfsOpsErr_INTERNAL;
         }
         else
         { // recovery succeeded
            LogContext(logContext).log(Log_NOTICE, "File session recovered.");
         }
      }
   } // end of session file session recovery attempt

   if(sessionFile)
   { // sessionFile exists (or was successfully recovered)
      FileInode* file = sessionFile->getInode();
      bool lockGranted = file->flockRange(lockDetails);
      if(!lockGranted)
         clientResult = FhgfsOpsErr_WOULDBLOCK;
      else
         clientResult = FhgfsOpsErr_SUCCESS;

      // cleanup
      sessionFiles->releaseSession(sessionFile, entryInfo);
   }


   // cleanup
cleanup_session:
   sessions->releaseSession(session);

   LOG_DEBUG(logContext, Log_SPAM, std::string("Client result: ") +
      FhgfsOpsErrTk::toErrString(clientResult) );

   // send response
   FLockRangeRespMsg respMsg(clientResult);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   return true;
}

