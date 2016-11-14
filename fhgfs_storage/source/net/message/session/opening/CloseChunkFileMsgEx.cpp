#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/toolkit/SessionTk.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include "CloseChunkFileMsgEx.h"

bool CloseChunkFileMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "CloseChunkFileMsg incoming";

   #ifdef BEEGFS_DEBUG
      LOG_DEBUG(logContext, Log_DEBUG, "Received a CloseChunkFileMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

   App* app = Program::getApp();
   Config* config = app->getConfig();
   SessionStore* sessions = app->getSessions();
   Session* session = sessions->referenceSession(getSessionID(), true);
   SessionLocalFileStore* sessionLocalFiles = session->getLocalFiles();
   SessionLocalFile* sessionLocalFile = NULL;

   uint16_t targetID;
   bool removeRes;

   FhgfsOpsErr closeMsgRes = FhgfsOpsErr_SUCCESS; // the result that will be sent to requestor
   DynamicAttribs dynAttribs;
   dynAttribs.storageVersion = 0; // 0 means undefined/invalid
   
   std::string fileHandleID(getFileHandleID() );
   bool isMirrorSession = isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR);


   // select the right targetID

   targetID = getTargetID();

   if(isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR) )
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND) ?
         mirrorBuddies->getSecondaryTargetID(targetID) :
         mirrorBuddies->getPrimaryTargetID(targetID);

      if(unlikely(!targetID) )
      { // unknown target
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            StringTk::uintToStr(getTargetID() ) );
         closeMsgRes = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }
   }

   // forward to secondary (if appropriate)

   closeMsgRes = forwardToSecondary(ctx);
   if(unlikely(closeMsgRes != FhgfsOpsErr_SUCCESS) )
   {
      if(closeMsgRes == FhgfsOpsErr_COMMUNICATION)
         goto skip_response; // GenericResponseMsg sent

      goto send_response;
   }

   removeRes = sessionLocalFiles->removeSession(
      fileHandleID, targetID, isMirrorSession, &sessionLocalFile);
   if(!removeRes)
   { // error
      LogContext(logContext).log(Log_DEBUG,
         "Cannot close chunk file (still in use, marked for delayed close). "
         "SessionID: " + getSessionID().str() + "; "
         "FileHandleID: " + fileHandleID);

      closeMsgRes = FhgfsOpsErr_INUSE;
   }

   // get current dynamic file attribs

   if(sessionLocalFile && (sessionLocalFile->getFD() != -1) )
   { // file open => refresh filesize and close file fd
      int fd = sessionLocalFile->getFD();

      /* get dynamic attribs, here before closing the file.
       * Note: Depending on the underlying file system the returned st_blocks might be too large
       *       (pre-allocated blocks, which are only released on close() ). Advantage here is
       *       that we already have the file descriptor. */
      if( (config->getTuneEarlyStat() ) &&
          (!isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS) ) )
         getDynamicAttribsByFD(fd, fileHandleID, targetID, dynAttribs);

      // set EntryInfo if requested
      if(isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_WRITEENTRYINFO) )
      {
         if (! StorageTkEx::attachEntryInfoToChunk(fd, getEntryInfoBuf(), getEntryInfoBufLen()) )
         {
            LogContext(logContext).logErr(std::string(
               "Cannot write entryinfo xattr for FileHandleID: " + fileHandleID));
         }
      }

      // close fd
      
      int closeRes = MsgHelperIO::close(fd);

      if(unlikely(closeRes) )
      { // error
         int sysErr = errno;

         LogContext(logContext).logErr("Unable to close chunk file. "
            "FD: " + StringTk::intToStr(fd) + "; "
            "HandleID: " + fileHandleID + "; "
            "SysErr: " + System::getErrString(sysErr) );

         closeMsgRes = FhgfsOpsErr_INTERNAL;
      }
      else
      { // success
         LogContext(logContext).log(Log_DEBUG, "Chunk file closed. "
            "FD: " + StringTk::intToStr(fd) + "; "
            "HandleID: " + fileHandleID);
      }

      // only get the attributes here, in order to make xfs to release pre-allocated blocks
      if( (!config->getTuneEarlyStat() ) &&
          (!isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS) ) )
         getDynamicAttribsByPath(fileHandleID, targetID, dynAttribs);

   }
   else
   if(!isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS) )
   { // file not open but exists => get dynamic attribs by path

      bool getRes = getDynamicAttribsByPath(fileHandleID, targetID, dynAttribs);
      if (getRes)
      {
         // LogContext(logContext).log(Log_DEBUG, "Chunk file virtually closed. "
         //   "HandleID: " + fileHandleID);
      }
      else
      {  // failed
         memset(&dynAttribs, 0, sizeof(dynAttribs) ); // avoid clang warning
         // LogContext(logContext).log(Log_DEBUG, "Getting dynAttribByPath failed");
      }
   }


   // note: "file not exists" is not an error. we just have nothing to do in that case.

   // cleanup

   SAFE_DELETE(sessionLocalFile);
   sessions->releaseSession(session);
   
   // send response

send_response:
   ctx.sendResponse(
         CloseChunkFileRespMsg(closeMsgRes, dynAttribs.filesize, dynAttribs.allocedBlocks,
            dynAttribs.modificationTimeSecs, dynAttribs.lastAccessTimeSecs,
            dynAttribs.storageVersion) );

skip_response:

   // update op counters

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), StorageOpCounter_CLOSELOCAL,
      getMsgHeaderUserID() );

   return true;
}

/**
 * If this is a buddy mirror msg and we are the primary, forward this msg to secondary.
 *
 * @return _COMMUNICATION if forwarding to buddy failed and buddy is not marked offline (in which
 *    case *outChunkLocked==false is guaranteed).
 * @throw SocketException if sending of GenericResponseMsg fails.
 */
FhgfsOpsErr CloseChunkFileMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   const char* logContext = "CloseChunkFileMsg incoming (forward to secondary)";

   App* app = Program::getApp();

   if(!isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR) ||
       isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND) )
      return FhgfsOpsErr_SUCCESS; // nothing to do

   // instead of creating a new msg object, we just re-use "this" with "buddymirror second" flag
   addMsgHeaderFeatureFlag(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND);

   RequestResponseArgs rrArgs(NULL, this, NETMSGTYPE_CloseChunkFileResp);
   RequestResponseTarget rrTarget(getTargetID(), app->getTargetMapper(), app->getStorageNodes(),
      app->getTargetStateStore(), app->getMirrorBuddyGroupMapper(), true);

   FhgfsOpsErr commRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

   // remove the flag that we just added for secondary
   unsetMsgHeaderFeatureFlag(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND);

   if(unlikely(
      (commRes == FhgfsOpsErr_COMMUNICATION) &&
      (rrTarget.outTargetReachabilityState == TargetReachabilityState_OFFLINE) ) )
   {
      LOG_DEBUG(logContext, Log_DEBUG, std::string("Secondary is offline and will need resync. ") +
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );;
      return FhgfsOpsErr_SUCCESS; // go ahead with local msg processing
   }

   if(unlikely(commRes != FhgfsOpsErr_SUCCESS) )
   {
      LogContext(logContext).log(Log_DEBUG, "Forwarding failed. "
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) + "; "
         "error: " + FhgfsOpsErrTk::toErrString(commRes) );

      std::string genericRespStr = "Communication with secondary failed. "
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() );

      ctx.sendResponse(
            GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR, genericRespStr.c_str() ) );

      return FhgfsOpsErr_COMMUNICATION;
   }

   CloseChunkFileRespMsg* respMsg = (CloseChunkFileRespMsg*)rrArgs.outRespMsg;
   FhgfsOpsErr secondaryRes = respMsg->getResult();
   if(unlikely(secondaryRes != FhgfsOpsErr_SUCCESS) )
   {
      LogContext(logContext).log(Log_NOTICE, std::string("Secondary reported error: ") +
         FhgfsOpsErrTk::toErrString(secondaryRes) + "; "
         "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );

      return secondaryRes;
   }


   return FhgfsOpsErr_SUCCESS;
}

bool CloseChunkFileMsgEx::getDynamicAttribsByFD(const int fd, std::string fileHandleID,
   uint16_t targetID, DynamicAttribs& outDynAttribs)
{
   SyncedStoragePaths* syncedPaths = Program::getApp()->getSyncedStoragePaths();

   std::string fileID(SessionTk::fileIDFromHandleID(fileHandleID) );

   uint64_t storageVersion = syncedPaths->lockPath(fileID, targetID); // LOCK
   
   // note: this is locked because we need to get the filesize together with the storageVersion
   bool getDynAttribsRes = StorageTkEx::getDynamicFileAttribs(fd, &outDynAttribs.filesize,
      &outDynAttribs.allocedBlocks, &outDynAttribs.modificationTimeSecs,
      &outDynAttribs.lastAccessTimeSecs);
   
   if(getDynAttribsRes)
      outDynAttribs.storageVersion = storageVersion;
   
   syncedPaths->unlockPath(fileID, targetID); // UNLOCK
   
   return getDynAttribsRes;
}

bool CloseChunkFileMsgEx::getDynamicAttribsByPath(std::string fileHandleID, uint16_t targetID,
   DynamicAttribs& outDynAttribs)
{
   const char* logContext = "CloseChunkFileMsg (attribs by path)";

   App* app = Program::getApp();
   SyncedStoragePaths* syncedPaths = app->getSyncedStoragePaths();

   int targetFD = app->getTargetFD(targetID,
      isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR) );
   if(unlikely(targetFD == -1) )
   { // unknown targetID
      LogContext(logContext).logErr("Unknown targetID: " + StringTk::uintToStr(targetID) );
      return false;
   }

   std::string fileID = SessionTk::fileIDFromHandleID(fileHandleID);
   std::string pathStr = StorageTk::getFileChunkPath(getPathInfo(), fileID);

   uint64_t storageVersion = syncedPaths->lockPath(fileID, targetID); // L O C K path
   
   // note: this is locked because we need to get the filesize together with the storageVersion
   bool getDynAttribsRes = StorageTkEx::getDynamicFileAttribs(targetFD, pathStr.c_str(),
      &outDynAttribs.filesize, &outDynAttribs.allocedBlocks, &outDynAttribs.modificationTimeSecs,
      &outDynAttribs.lastAccessTimeSecs);
   
   if(getDynAttribsRes)
      outDynAttribs.storageVersion = storageVersion;
   
   syncedPaths->unlockPath(fileID, targetID); // U N L O C K path
   
   return getDynAttribsRes;
}
