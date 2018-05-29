#include <program/Program.h>
#include <common/net/message/session/FSyncLocalFileRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <net/msghelpers/MsgHelperIO.h>
#include "FSyncLocalFileMsgEx.h"


bool FSyncLocalFileMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "FSyncLocalFileMsg incoming";

   #ifdef BEEGFS_DEBUG
      LOG_DEBUG(logContext, Log_DEBUG,
         "Received a FSyncLocalFileMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

   FhgfsOpsErr clientRes = FhgfsOpsErr_SUCCESS;
   bool isMirrorSession = isMsgHeaderFeatureFlagSet(FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR);

   // do session check only when it is not a mirror session
   bool useSessionCheck = isMirrorSession ? false :
      isMsgHeaderFeatureFlagSet(FSYNCLOCALFILEMSG_FLAG_SESSION_CHECK);

   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();
   Session* session = sessions->referenceSession(getSessionID(), true);
   SessionLocalFileStore* sessionLocalFiles = session->getLocalFiles();

   // select the right targetID

   uint16_t targetID = getTargetID();

   if(isMirrorSession)
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = isMsgHeaderFeatureFlagSet(FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) ?
         mirrorBuddies->getSecondaryTargetID(targetID) :
         mirrorBuddies->getPrimaryTargetID(targetID);

      // note: only log message here, error handling will happen below through invalid targetFD
      if(unlikely(!targetID) )
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            StringTk::uintToStr(getTargetID() ) );
   }

   SessionLocalFile* sessionLocalFile =
      sessionLocalFiles->referenceSession(getFileHandleID(), targetID, isMirrorSession);

   if(sessionLocalFile)
   { // sessionLocalFile exists => check if open and perform fsync
      if (!isMsgHeaderFeatureFlagSet(FSYNCLOCALFILEMSG_FLAG_NO_SYNC) )
      {
         int fd = sessionLocalFile->getFD();
         if(fd != -1)
         { // file open => sync
            int fsyncRes = MsgHelperIO::fsync(fd);

            if(fsyncRes)
            {
               LogContext log(logContext);
               log.log(Log_WARNING, std::string("fsync of chunk file failed. ") +
                  std::string("SessionID: ") + getSessionID().str() +
                  std::string(". SysErr: ") + System::getErrString() );

               clientRes = FhgfsOpsErr_INTERNAL;
            }
         }

      }

      if(useSessionCheck && sessionLocalFile->isServerCrashed() )
      { // server crashed during the write, maybe lost some data send error to client
         LogContext log(logContext);
         log.log(Log_SPAM, "Potential cache loss for open file handle. (Server crash detected.) "
            "The session is marked as dirty.");
         clientRes = FhgfsOpsErr_STORAGE_SRV_CRASHED;
      }

      sessionLocalFiles->releaseSession(sessionLocalFile);
   }
   else
   if (useSessionCheck)
   { // the server crashed during a write or before the close was successful
      LogContext log(logContext);
      log.log(Log_WARNING, "Potential cache loss for open file handle. (Server crash detected.) "
         "No session for file available. "
         "FileHandleID: " + std::string(getFileHandleID()) );

      clientRes = FhgfsOpsErr_STORAGE_SRV_CRASHED;
   }

   sessions->releaseSession(session);

   ctx.sendResponse(FSyncLocalFileRespMsg(clientRes) );

   return true;
}


