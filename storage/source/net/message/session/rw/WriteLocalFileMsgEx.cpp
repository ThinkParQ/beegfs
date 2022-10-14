#include <program/Program.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SessionTk.h>
#include <common/toolkit/StorageTk.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <storage/StorageTargets.h>
#include <toolkit/StorageTkEx.h>
#include "WriteLocalFileMsgEx.h"
#ifdef BEEGFS_NVFS
#include "WriteLocalFileRDMAMsgEx.h"
#endif

#include <boost/lexical_cast.hpp>

static WriteLocalFileMsgEx forcedLinkage;
#ifdef BEEGFS_NVFS
static WriteLocalFileRDMAMsgEx forcedLinkageRDMA;
#endif

const std::string WriteLocalFileMsgSender::logContextPref = "WriteChunkFileMsg";
#ifdef BEEGFS_NVFS
const std::string WriteLocalFileRDMAMsgSender::logContextPref = "WriteChunkFileRDMAMsg";
#endif

template <class Msg, typename WriteState>
bool WriteLocalFileMsgExBase<Msg, WriteState>::processIncoming(NetMessage::ResponseContext& ctx)
{
   App* app = Program::getApp();

   bool success;
   int64_t writeClientRes;

   if (!isMsgValid())
   {
      sendResponse(ctx, FhgfsOpsErr_INVAL);
      return false;
   }

   std::tie(success, writeClientRes) = write(ctx);

   if (success)
   {
      sendResponse(ctx, writeClientRes);

      // update operation counters

      if (likely(writeClientRes > 0))
         app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
               StorageOpCounter_WRITEOPS, writeClientRes, getMsgHeaderUserID());
   }

   return success;
}

template <class Msg, typename WriteState>
std::pair<bool, int64_t> WriteLocalFileMsgExBase<Msg, WriteState>::write(NetMessage::ResponseContext& ctx)
{
   std::string logContext = Msg::logContextPref + " incoming";

   App* app = Program::getApp();

   int64_t writeClientRes = -(int64_t)FhgfsOpsErr_INTERNAL; // bytes written or negative fhgfs err
   FhgfsOpsErr finishMirroringRes = FhgfsOpsErr_INTERNAL;
   std::string fileHandleID(getFileHandleID() );
   bool isMirrorSession = isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR);

   bool serverCrashed = false;
   QuotaExceededErrorType quotaExceeded = QuotaExceededErrorType_NOT_EXCEEDED;

   SessionStore* sessions = Program::getApp()->getSessions();
   auto session = sessions->referenceOrAddSession(getClientNumID());
   SessionLocalFileStore* sessionLocalFiles = session->getLocalFiles();

   ChunkLockStore* chunkLockStore = app->getChunkLockStore();
   bool chunkLocked = false;

   // select the right targetID

   uint16_t targetID = getTargetID();

   if(isMirrorSession)
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) ?
         mirrorBuddies->getSecondaryTargetID(targetID) :
         mirrorBuddies->getPrimaryTargetID(targetID);

      // note: only log message here, error handling will happen below through invalid targetFD
      if(unlikely(!targetID) )
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            StringTk::uintToStr(getTargetID() ) );
   }

   auto* const target = app->getStorageTargets()->getTarget(targetID);
   if (!target)
   {
      if (isMirrorSession)
      { /* buddy mirrored file => fail with Err_COMMUNICATION to make the requestor retry.
           mgmt will mark this target as (p)offline in a few moments. */
         LOG(GENERAL, NOTICE, "Unknown target ID, refusing request.", targetID);
         return {false, FhgfsOpsErr_COMMUNICATION};
      }

      LOG(GENERAL, ERR, "Unknown target ID.", targetID);
      return {false, FhgfsOpsErr_UNKNOWNTARGET};
   }

   // check if we already have session for this file...

   auto sessionLocalFile = sessionLocalFiles->referenceSession(
      fileHandleID, targetID, isMirrorSession);

   if(!sessionLocalFile)
   { // sessionLocalFile not exists yet => create, insert, re-get it

      if(doSessionCheck() )
      { // server crashed during the write, maybe lost some data send error to client
         LogContext log(logContext);
         log.log(Log_WARNING, "Potential cache loss for open file handle. (Server crash detected.) "
            "No session for file available. "
            "FileHandleID: " + fileHandleID);

         serverCrashed = true;
      }

      std::string fileID = SessionTk::fileIDFromHandleID(fileHandleID);
      int openFlags = SessionTk::sysOpenFlagsFromFhgfsAccessFlags(getAccessFlags() );

      auto newFile = boost::make_unique<SessionLocalFile>(fileHandleID, targetID, fileID, openFlags,
         serverCrashed);

      if(isMirrorSession)
         newFile->setIsMirrorSession(true);

      sessionLocalFile = sessionLocalFiles->addAndReferenceSession(std::move(newFile));
   }
   else
   { // session file exists

      if(doSessionCheck() && sessionLocalFile->isServerCrashed() )
      { // server crashed during the write, maybe lost some data send error to client
         LogContext log(logContext);
         log.log(Log_SPAM, "Potential cache loss for open file handle. (Server crash detected.)"
            "The session is marked as dirty. "
            "FileHandleID: " + fileHandleID);

         serverCrashed = true;
      }
   }

   // check if the size quota is exceeded for the user or group
   if(isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_USE_QUOTA) &&
      app->getConfig()->getQuotaEnableEnforcement() )
   {
      quotaExceeded = app->getExceededQuotaStores()->get(targetID)->isQuotaExceeded(getUserID(),
         getGroupID(), QuotaLimitType_SIZE);

      if(quotaExceeded != QuotaExceededErrorType_NOT_EXCEEDED)
      {
         LogContext(logContext).log(Log_NOTICE,
            QuotaData::QuotaExceededErrorTypeToString(quotaExceeded) + " "
            "UID: " + StringTk::uintToStr(this->getUserID()) + "; "
            "GID: " + StringTk::uintToStr(this->getGroupID() ) );

         // receive the message content before return with error
         incrementalRecvPadding(ctx, getCount(), sessionLocalFile.get());
         writeClientRes = -(int64_t) FhgfsOpsErr_DQUOT;
         goto cleanup;
      }
   }

   try
   {
      if(isMirrorSession && target->getBuddyResyncInProgress())
      {
         // mirrored chunk should be modified, check if resync is in progress and lock chunk
         std::string chunkID = sessionLocalFile->getFileID();
         chunkLockStore->lockChunk(targetID, chunkID);
         chunkLocked = true;
      }

      // prepare file descriptor (if file not open yet then create/open it)
      FhgfsOpsErr openRes = openFile(*target, sessionLocalFile.get());
      if(unlikely(openRes != FhgfsOpsErr_SUCCESS) )
      {
         incrementalRecvPadding(ctx, getCount(), sessionLocalFile.get());
         writeClientRes = -(int64_t)openRes;
         goto cleanup;
      }

      // store mirror node reference in session and init mirrorToSock member
      FhgfsOpsErr prepMirrorRes = prepareMirroring(ctx.getBuffer(), ctx.getBufferLength(),
         sessionLocalFile.get(), *target);
      if(unlikely(prepMirrorRes != FhgfsOpsErr_SUCCESS) )
      { // mirroring failed
         incrementalRecvPadding(ctx, getCount(), sessionLocalFile.get());
         writeClientRes = -(int64_t)prepMirrorRes;
         goto cleanup;
      }


      // the actual write workhorse

      int64_t writeLocalRes = incrementalRecvAndWriteStateful(ctx, sessionLocalFile.get());

      // update client result, offset etc.

      int64_t newOffset;

      if(unlikely(writeLocalRes < 0) )
         newOffset = -1; // writing failed
      else
      { // writing succeeded
         newOffset = getOffset() + writeLocalRes;
         ctx.getStats()->incVals.diskWriteBytes += writeLocalRes; // update stats
      }

      sessionLocalFile->setOffset(newOffset);

      writeClientRes = writeLocalRes;

   }
   catch(SocketException& e)
   {
      LogContext(logContext).logErr(std::string("SocketException occurred: ") + e.what() );
      LogContext(logContext).log(Log_WARNING, std::string("Details: ") +
         "sessionID: " + getClientNumID().str() + "; "
         "fileHandle: " + std::string(sessionLocalFile->getFileHandleID() ) + "; "
         "offset: " + StringTk::int64ToStr(getOffset() ) + "; "
         "count: " + StringTk::int64ToStr(getCount() ) );

      sessionLocalFile->setOffset(-1); // invalidate offset

      finishMirroring(sessionLocalFile.get(), *target);

      if (chunkLocked)
      {
         std::string chunkID = sessionLocalFile->getFileID();
         chunkLockStore->unlockChunk(targetID, chunkID);
      }

      return {false, -1};
   }


cleanup:
   finishMirroringRes = finishMirroring(sessionLocalFile.get(), *target);

   // check mirroring result (don't overwrite local error code, if any)
   if(likely(writeClientRes > 0) )
   { // no local error => check mirroring result
      if(unlikely(finishMirroringRes != FhgfsOpsErr_SUCCESS) )
         writeClientRes = -finishMirroringRes; // mirroring failed => use err code as client result
   }

   if (chunkLocked)
   {
      std::string chunkID = sessionLocalFile->getFileID();
      chunkLockStore->unlockChunk(targetID, chunkID);
   }

   if (serverCrashed)
      writeClientRes = -(int64_t) FhgfsOpsErr_STORAGE_SRV_CRASHED;

   return {true, writeClientRes};
}

ssize_t WriteLocalFileMsgSender::recvPadding(ResponseContext& ctx, int64_t toBeReceived)
{
   Config* cfg = Program::getApp()->getConfig();
   return ctx.getSocket()->recvT(ctx.getBuffer(),
         BEEGFS_MIN(toBeReceived, ctx.getBufferLength()), 0, cfg->getConnMsgMediumTimeout());
}

#ifdef BEEGFS_NVFS

ssize_t WriteLocalFileRDMAMsgSender::recvPadding(ResponseContext& ctx, int64_t toBeReceived)
{
   RdmaInfo* rdma = getRdmaInfo();
   uint64_t rBuf;
   size_t rLen;
   uint64_t rOff;

   if (!rdma->next(rBuf, rLen, rOff))
      return -1;

   ssize_t recvLength = BEEGFS_MIN(ctx.getBufferLength(), toBeReceived);
   recvLength = BEEGFS_MIN(recvLength, (ssize_t)(rLen - rOff));
   return ctx.getSocket()->read(ctx.getBuffer(), recvLength, 0, rBuf+rOff, rdma->key);
}

#endif /* BEEGFS_NVFS */

/**
 * Note: New offset is saved in the session by the caller afterwards (to make life easier).
 * @return number of written bytes or negative fhgfs error code
 */
template <class Msg, typename WriteState>
int64_t WriteLocalFileMsgExBase<Msg, WriteState>::incrementalRecvAndWriteStateful(NetMessage::ResponseContext& ctx,
   SessionLocalFile* sessionLocalFile)
{
   std::string logContext = Msg::logContextPref + " (write incremental)";
   Config* cfg = Program::getApp()->getConfig();

   // we can securely cast getTuneFileWriteSize to size_t below to make a comparision possible, as
   // it can technically never be negative and will therefore always fit into size_t
   const ssize_t exactStaticRecvSize = sessionLocalFile->getIsDirectIO()
      ? ctx.getBufferLength()
      : BEEGFS_MIN(ctx.getBufferLength(), (size_t)cfg->getTuneFileWriteSize() );

   auto& fd = sessionLocalFile->getFD();

   int64_t oldOffset = sessionLocalFile->getOffset();
   int64_t newOffset = getOffset();
   bool useSyncRange = false; // true if sync_file_range should be called

   if( (oldOffset < 0) || (oldOffset != newOffset) )
      sessionLocalFile->resetWriteCounter(); // reset sequential write counter
   else
   { // continue at previous offset => increase sequential write counter
      LOG_DEBUG(logContext, Log_SPAM, "Offset: " + StringTk::int64ToStr(getOffset() ) );

      sessionLocalFile->incWriteCounter(getCount() );

      ssize_t syncSize = unlikely(isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_DISABLE_IO) ) ?
         0 : cfg->getTuneFileWriteSyncSize();
      if (syncSize && (sessionLocalFile->getWriteCounter() >= syncSize) )
         useSyncRange = true;
   }

   // incrementally receive file contents...

   WriteState writeState(logContext.c_str(), exactStaticRecvSize,
      getCount(), getOffset(), sessionLocalFile);
   if (!writeStateInit(writeState))
      return -FhgfsOpsErr_COMMUNICATION;

   do
   {
      // receive some bytes...

      LOG_DEBUG(logContext, Log_SPAM,
         "receiving... (remaining: " + StringTk::intToStr(writeState.toBeReceived) + ")");

      ssize_t recvRes = writeStateRecvData(ctx, writeState);
      if (recvRes < 0)
      {
         LogContext(logContext).log(Log_WARNING, "Socket data transfer error occurred. ");
         return -FhgfsOpsErr_COMMUNICATION;
      }

      // forward to mirror...

      FhgfsOpsErr mirrorRes = sendToMirror(ctx.getBuffer(), recvRes,
         writeState.writeOffset, writeState.toBeReceived, sessionLocalFile);
      if(unlikely(mirrorRes != FhgfsOpsErr_SUCCESS) )
      { // mirroring failed
         incrementalRecvPadding(ctx, writeState.toBeReceived, sessionLocalFile);

         return -FhgfsOpsErr_COMMUNICATION;
      }

      // write to underlying file system...

      int errCode = 0;
      ssize_t writeRes = unlikely(isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_DISABLE_IO) )
         ? recvRes
         : doWrite(*fd, ctx.getBuffer(), recvRes, writeState.writeOffset, errCode);

      writeState.toBeReceived -= recvRes;

      // handle write errors...

      if(unlikely(writeRes != recvRes) )
      { // didn't write all of the received data

         if(writeRes == -1)
         { // write error occurred
            LogContext(logContext).log(Log_WARNING, "Write error occurred. "
               "FileHandleID: " + sessionLocalFile->getFileHandleID() + "."
               "Target: " + StringTk::uintToStr(sessionLocalFile->getTargetID() ) + ". "
               "File: " + sessionLocalFile->getFileID() + ". "
               "SysErr: " + System::getErrString(errCode) );
            LogContext(logContext).log(Log_NOTICE, std::string("Additional info: "
               "FD: ") + StringTk::intToStr(*fd) + " " +
               "OpenFlags: " + StringTk::intToStr(sessionLocalFile->getOpenFlags() ) + " " +
               "received: " + StringTk::intToStr(recvRes) + ".");

            incrementalRecvPadding(ctx, writeState.toBeReceived, sessionLocalFile);

            return -FhgfsOpsErrTk::fromSysErr(errCode);
         }
         else
         { // wrote only a part of the data, not all of it
            LogContext(logContext).log(Log_WARNING,
               "Unable to write all of the received data. "
               "target: " + StringTk::uintToStr(sessionLocalFile->getTargetID() ) + "; "
               "file: " + sessionLocalFile->getFileID() + "; "
               "sysErr: " + System::getErrString(errCode) );

            incrementalRecvPadding(ctx, writeState.toBeReceived, sessionLocalFile);

            // return bytes received so far minus num bytes that were not written with last write
            return (getCount() - writeState.toBeReceived) - (recvRes - writeRes);
         }

      }

      writeState.writeOffset += writeRes;
      recvRes = writeStateNext(writeState, writeRes);
      if (recvRes != 0)
         return recvRes;
   } while(writeState.toBeReceived);

   LOG_DEBUG(logContext, Log_SPAM,
      std::string("Received and wrote all the data") );

   // commit to storage device queue...

   if (useSyncRange)
   {
      // advise kernel to commit written data to storage device in max_sectors_kb chunks.

      /* note: this is async if there are free slots in the request queue
         /sys/block/<...>/nr_requests. (optimal_io_size is not honoured as of linux-3.4) */

      off64_t syncSize = sessionLocalFile->getWriteCounter();
      off64_t syncOffset = getOffset() + getCount() - syncSize;

      MsgHelperIO::syncFileRange(*fd, syncOffset, syncSize);
      sessionLocalFile->resetWriteCounter();
   }

   return getCount();
}

/**
 * Write until everything was written (handle short-writes) or an error occured
 */
template <class Msg, typename WriteState>
ssize_t WriteLocalFileMsgExBase<Msg, WriteState>::doWrite(int fd, char* buf, size_t count, off_t offset, int& outErrno)
{
   size_t sumWriteRes = 0;

   do
   {
      ssize_t writeRes =
         MsgHelperIO::pwrite(fd, buf + sumWriteRes, count - sumWriteRes, offset + sumWriteRes);

      if (unlikely(writeRes == -1) )
      {
         sumWriteRes = (sumWriteRes > 0) ? sumWriteRes : writeRes;
         outErrno = errno;
         break;
      }

      sumWriteRes += writeRes;

   } while (sumWriteRes != count);

   return sumWriteRes;
}

/**
 * Receive and discard data.
 */
template <class Msg, typename WriteState>
void WriteLocalFileMsgExBase<Msg, WriteState>::incrementalRecvPadding(NetMessage::ResponseContext& ctx,
   int64_t padLen, SessionLocalFile* sessionLocalFile)
{
   uint64_t toBeReceived = padLen;

   while(toBeReceived)
   {
      ssize_t recvRes = recvPadding(ctx, toBeReceived);
      if (recvRes == -1)
         break;
      // forward to mirror...

      FhgfsOpsErr mirrorRes = sendToMirror(ctx.getBuffer(), recvRes,
         getOffset() + padLen - toBeReceived, toBeReceived, sessionLocalFile);
      if(unlikely(mirrorRes != FhgfsOpsErr_SUCCESS) )
      { // mirroring failed
         /* ... but if we are in this method, then something went wrong anyways, so don't set
            needs-resync here or report any error to caller. */
      }

      toBeReceived -= recvRes;
   }
}

template <class Msg, typename WriteState>
FhgfsOpsErr WriteLocalFileMsgExBase<Msg, WriteState>::openFile(const StorageTarget& target,
      SessionLocalFile* sessionLocalFile)
{
   std::string logContext = Msg::logContextPref + " (write incremental)";

   bool useQuota = isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_USE_QUOTA);
   bool enforceQuota = Program::getApp()->getConfig()->getQuotaEnableEnforcement();

   bool isBuddyMirrorChunk = sessionLocalFile->getIsMirrorSession();


   if (sessionLocalFile->getFD().valid())
      return FhgfsOpsErr_SUCCESS; // file already open => nothing to be done here


   // file not open yet => get targetFD and check consistency state

   const auto consistencyState = target.getConsistencyState();
   const int targetFD = isBuddyMirrorChunk ? *target.getMirrorFD() : *target.getChunkFD();

   if(unlikely(consistencyState != TargetConsistencyState_GOOD) &&
      isBuddyMirrorChunk &&
      !isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) )
   { // this is a request for a buddymirrored chunk on a non-good primary
      LogContext(logContext).log(Log_NOTICE, "Refusing request. Target consistency is not good. "
         "targetID: " + StringTk::uintToStr(target.getID()));

      return FhgfsOpsErr_COMMUNICATION;
   }

   SessionQuotaInfo quotaInfo(useQuota, enforceQuota, getUserID(), getGroupID() );

   FhgfsOpsErr openChunkRes = sessionLocalFile->openFile(targetFD, getPathInfo(), true, &quotaInfo);

   return openChunkRes;
}


/**
 * Prepares mirroring by storing mirrorNode reference in file session and setting the mirrorToSock
 * member variable.
 *
 * Note: Mirror node reference needs to be released on file session close.
 *
 * @param buf used to send initial write msg header to mirror.
 * @param requestorSock used to receive padding if mirroring fails.
 * @return FhgfsOpsErr_COMMUNICATION if communication with mirror failed.
 */
template <class Msg, typename WriteState>
FhgfsOpsErr WriteLocalFileMsgExBase<Msg, WriteState>::prepareMirroring(char* buf, size_t bufLen,
   SessionLocalFile* sessionLocalFile, StorageTarget& target)
{
   std::string logContext = Msg::logContextPref + " (prepare mirroring)";

   // check if mirroring is enabled

   if(!isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_FORWARD) )
      return FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();
   TargetStateStore* targetStates = app->getTargetStateStore();

   // check if secondary is offline or in unclear state

   uint16_t secondaryTargetID = mirrorBuddies->getSecondaryTargetID(getTargetID() );
   if(unlikely(!secondaryTargetID) )
   {
      LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
         StringTk::uintToStr(getTargetID() ) );

      return FhgfsOpsErr_UNKNOWNTARGET;
   }

   CombinedTargetState secondaryState;

   bool getSecondaryStateRes = targetStates->getState(secondaryTargetID, secondaryState);
   if(unlikely(!getSecondaryStateRes) )
   {
      LOG_DEBUG(logContext, Log_DEBUG,
         "Refusing request. Secondary target has invalid state. "
         "targetID: " + StringTk::uintToStr(secondaryTargetID) );
      return FhgfsOpsErr_COMMUNICATION;
   }

   if( (secondaryState.reachabilityState != TargetReachabilityState_ONLINE) ||
       (secondaryState.consistencyState != TargetConsistencyState_GOOD) )
   {
      if(secondaryState.reachabilityState == TargetReachabilityState_OFFLINE)
      { // buddy is offline => mark needed resync and continue with local operation
         LOG_DEBUG(logContext, Log_DEBUG,
            "Secondary is offline and will need resync. "
            "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );

         // buddy is marked offline, so local msg processing will be done and buddy needs resync

         target.setBuddyNeedsResync(true);

         return FhgfsOpsErr_SUCCESS;
      }

      if(secondaryState.consistencyState != TargetConsistencyState_NEEDS_RESYNC)
      { // unclear buddy state => client must try again
         LOG_DEBUG(logContext, Log_DEBUG,
            "Unclear secondary state, caller will have to try again later. "
            "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );

         return FhgfsOpsErr_COMMUNICATION;
      }
   }


   // store mirror node reference in session...

   NodeHandle mirrorToNode = sessionLocalFile->getMirrorNode();

   if(!mirrorToNode)
   {
      NodeStoreServers* storageNodes = app->getStorageNodes();
      TargetMapper* targetMapper = app->getTargetMapper();
      FhgfsOpsErr referenceErr;

      mirrorToNode = storageNodes->referenceNodeByTargetID(secondaryTargetID, targetMapper,
         &referenceErr);

      if(unlikely(referenceErr != FhgfsOpsErr_SUCCESS) )
      {
         LogContext(logContext).logErr(
            "Unable to forward to mirror target: " + StringTk::uintToStr(secondaryTargetID) + "; "
            "Error: " + boost::lexical_cast<std::string>(referenceErr));
         return referenceErr;
      }

      mirrorToNode = sessionLocalFile->setMirrorNodeExclusive(mirrorToNode);
   }

   // send initial write msg header to mirror (retry loop)...

   for( ; ; )
   {
      try
      {
         // acquire connection to mirror node and send write msg...

         mirrorToSock = mirrorToNode->getConnPool()->acquireStreamSocket();

         WriteLocalFileMsg mirrorWriteMsg(getClientNumID(), getFileHandleID(), getTargetID(),
            getPathInfo(), getAccessFlags(), getOffset(), getCount());

         if(doSessionCheck() )
            mirrorWriteMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_SESSION_CHECK);

         if(isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_DISABLE_IO) )
            mirrorWriteMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_DISABLE_IO);

         if(isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_USE_QUOTA) )
            mirrorWriteMsg.setUserdataForQuota(getUserID(), getGroupID() );

         mirrorWriteMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR);
         mirrorWriteMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);

         unsigned msgLength = mirrorWriteMsg.serializeMessage(buf, bufLen).second;
         mirrorToSock->send(buf, msgLength, 0);

         return FhgfsOpsErr_SUCCESS;
      }
      catch(SocketConnectException& e)
      {
         LogContext(logContext).log(Log_CRITICAL, "Unable to connect to mirror node: " +
            mirrorToNode->getNodeIDWithTypeStr() + "; "
            "Msg: " + e.what() );
      }
      catch(SocketException& e)
      {
         LogContext(logContext).log(Log_CRITICAL, "Communication with mirror node failed: " +
            mirrorToNode->getNodeIDWithTypeStr() + "; "
            "Msg: " + e.what() );

         if(mirrorToSock)
            mirrorToNode->getConnPool()->invalidateStreamSocket(mirrorToSock);

         mirrorToSock = NULL;
      }

      // error occurred if we got here

      if(!mirrorRetriesLeft)
         break;

      mirrorRetriesLeft--;

      // next round will be a retry
      LogContext(logContext).log(Log_NOTICE, "Retrying mirror communication: " +
         mirrorToNode->getNodeIDWithTypeStr() );

   } // end of retry for-loop


   // all retries exhausted if we got here

   return FhgfsOpsErr_COMMUNICATION;
}

/**
 * Send file contents to mirror.
 *
 * Note: Supports retries only at beginning of write msg.
 *
 * @param buf the buffer that should be sent to the mirror.
 * @param offset the offset within the chunk file (only used if communication fails and we need to
 * start over with a new WriteMsg to the mirror).
 * @param toBeMirrored total remaining mirror data including given bufLen (only used for retries).
 * @return FhgfsOpsErr_COMMUNICATION if mirroring fails.
 */
template <class Msg, typename WriteState>
FhgfsOpsErr WriteLocalFileMsgExBase<Msg, WriteState>::sendToMirror(const char* buf, size_t bufLen,
   int64_t offset, int64_t toBeMirrored, SessionLocalFile* sessionLocalFile)
{
   std::string logContext = Msg::logContextPref + " (send to mirror)";

   // check if mirroring enabled

   if(!mirrorToSock)
      return FhgfsOpsErr_SUCCESS; // either no mirroring enabled or all retries exhausted

   bool isRetryRound = false;

   // send raw data (retry loop)...
   // (note: if sending fails, retrying requires sending of a new WriteMsg)

   for( ; ; )
   {
      try
      {
         if(unlikely(isRetryRound) )
         { // retry requires reconnect and resend of write msg with current offset

            auto mirrorToNode = sessionLocalFile->getMirrorNode();

            mirrorToSock = mirrorToNode->getConnPool()->acquireStreamSocket();

            WriteLocalFileMsg mirrorWriteMsg(getClientNumID(), getFileHandleID(),
               getTargetID(), getPathInfo(), getAccessFlags(), offset, toBeMirrored);

            if(doSessionCheck() )
               mirrorWriteMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_SESSION_CHECK);

            if(isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_DISABLE_IO) )
               mirrorWriteMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_DISABLE_IO);

            if(isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_USE_QUOTA) )
               mirrorWriteMsg.setUserdataForQuota(getUserID(), getGroupID() );

            mirrorWriteMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR);
            mirrorWriteMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);

            const auto mirrorBuf = MessagingTk::createMsgVec(mirrorWriteMsg);

            mirrorToSock->send(&mirrorBuf[0], mirrorBuf.size(), 0);
         }

         mirrorToSock->send(buf, bufLen, 0);

         return FhgfsOpsErr_SUCCESS;
      }
      catch(SocketConnectException& e)
      {
         auto mirrorToNode = sessionLocalFile->getMirrorNode();

         LogContext(logContext).log(Log_CRITICAL, "Unable to connect to mirror node: " +
            mirrorToNode->getNodeIDWithTypeStr() + "; "
            "Msg: " + e.what() );
      }
      catch(SocketException& e)
      {
         LogContext(logContext).log(Log_CRITICAL, "Communication with mirror node failed: " +
            sessionLocalFile->getMirrorNode()->getNodeIDWithTypeStr() + "; "
            "Msg: " + e.what() );

         if(mirrorToSock)
            sessionLocalFile->getMirrorNode()->getConnPool()->invalidateStreamSocket(mirrorToSock);

         mirrorToSock = NULL;
      }

      // error occurred if we got here

      if(!mirrorRetriesLeft)
         break;

      // only allow retries if we're still at the beginning of the write msg.
      /* (this is because later we don't have all the client data available; and without the mirror
         response we don't know for sure whether previously sent data was really written or not.) */
      if(toBeMirrored != getCount() )
         break;

      mirrorRetriesLeft--;

      // next round will be a retry
      LogContext(logContext).log(Log_NOTICE, "Retrying mirror communication: " +
         sessionLocalFile->getMirrorNode()->getNodeIDWithTypeStr() );

      isRetryRound = true;

   } // end of retry for-loop

   // all retries exhausted if we got here

   return FhgfsOpsErr_COMMUNICATION;
}

/**
 * Receive response from mirror node, check result, clean up (release mirror sock).
 *
 * Note: Does not do retries on communication errors
 */
template <class Msg, typename WriteState>
FhgfsOpsErr WriteLocalFileMsgExBase<Msg, WriteState>::finishMirroring(SessionLocalFile* sessionLocalFile,
      StorageTarget& target)
{
   std::string logContext = Msg::logContextPref + " (finish mirroring)";

   // check if mirroring enabled

   if(!mirrorToSock)
      return FhgfsOpsErr_SUCCESS; // mirroring disabled

   App* app = Program::getApp();
   auto mirrorToNode = sessionLocalFile->getMirrorNode();

   WriteLocalFileRespMsg* writeRespMsg;
   int64_t mirrorWriteRes;


   // receive write msg response from mirror...
   /* note: we don't have the file contents that were sent by the client anymore at this point, so
      we cannot do retries here with a new WriteMsg. */

   try
   {
      // receive write msg response...

      auto resp = MessagingTk::recvMsgBuf(*mirrorToSock);
      if (resp.empty())
      { // error
          LogContext(logContext).log(Log_WARNING,
             "Failed to receive response from mirror: " + mirrorToSock->getPeername() );

          goto cleanup_commerr;
      }

      // got response => deserialize it...

      auto respMsg = app->getNetMessageFactory()->createFromBuf(std::move(resp));

      if(unlikely(respMsg->getMsgType() != NETMSGTYPE_WriteLocalFileResp) )
      { // response invalid (wrong msgType)
         LogContext(logContext).logErr(
            "Received invalid response type: " + StringTk::intToStr(respMsg->getMsgType() ) +"; "
            "expected type: " + StringTk::intToStr(NETMSGTYPE_WriteLocalFileResp) + ". "
            "Disconnecting: " + mirrorToSock->getPeername() );

         goto cleanup_commerr;
      }

      // check mirror result and release mirror socket...

      mirrorToNode->getConnPool()->releaseStreamSocket(mirrorToSock);

      writeRespMsg = (WriteLocalFileRespMsg*)respMsg.get();
      mirrorWriteRes = writeRespMsg->getValue();

      if(likely(mirrorWriteRes == getCount() ) )
         return FhgfsOpsErr_SUCCESS; // mirror successfully wrote all of the data

      if(mirrorWriteRes >= 0)
      { // mirror only wrote a part of the data
         LogContext(logContext).log(Log_WARNING,
            "Mirror did not write all of the data (no space left); "
            "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) + "; "
            "fileHandle: " + sessionLocalFile->getFileHandleID() );

         return FhgfsOpsErr_NOSPACE;
      }

      if(mirrorWriteRes == -FhgfsOpsErr_UNKNOWNTARGET)
      {
         /* local msg processing shall be done and buddy needs resync
            (this is normal when a storage is restarted without a broken secondary target, so we
            report success to a client in this case) */

         LogContext(logContext).log(Log_DEBUG,
            "Secondary reports unknown target error and will need resync. "
            "mirror buddy group ID: " + StringTk::uintToStr(getTargetID() ) );

         target.setBuddyNeedsResync(true);

         return FhgfsOpsErr_SUCCESS;
      }

      if(mirrorWriteRes == -FhgfsOpsErr_STORAGE_SRV_CRASHED)
         LogContext(logContext).log(Log_NOTICE, "Potential cache loss for open file handle. "
            "(Mirror server crash detected.) "
            "FileHandleID: " + sessionLocalFile->getFileHandleID() + "; "
            "Mirror: " + mirrorToNode->getNodeIDWithTypeStr() );

      // mirror encountered an error
      return (FhgfsOpsErr)-mirrorWriteRes; // write response contains negative fhgfs error code

   }
   catch(SocketException& e)
   {
      LogContext(logContext).logErr(std::string("SocketException: ") + e.what() );
      LogContext(logContext).log(Log_WARNING, "Additional info: "
         "mirror node: " + mirrorToNode->getNodeIDWithTypeStr() + "; "
         "fileHandle: " + sessionLocalFile->getFileHandleID() );
   }


   // cleanup after communication error...

cleanup_commerr:
   mirrorToNode->getConnPool()->invalidateStreamSocket(mirrorToSock);

   return FhgfsOpsErr_COMMUNICATION;
}

template <class Msg, typename WriteState>
bool WriteLocalFileMsgExBase<Msg, WriteState>::doSessionCheck()
{ // do session check only when it is not a mirror session
   return isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_BUDDYMIRROR) ? false :
      isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_SESSION_CHECK);
}
