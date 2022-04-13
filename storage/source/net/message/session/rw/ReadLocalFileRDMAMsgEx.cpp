#ifdef BEEGFS_NVFS
#include <program/Program.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SessionTk.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <toolkit/StorageTkEx.h>
#include "ReadLocalFileRDMAMsgEx.h"

#include <sys/sendfile.h>
#include <sys/mman.h>

#define READ_USE_TUNEFILEREAD_TRIGGER   (4*1024*1024)  /* seq IO trigger for tuneFileReadSize */

#define READ_BUF_OFFSET_PROTO_MIN   (sizeof(int64_t) ) /* for prepended length info */
#define READ_BUF_END_PROTO_MIN      (sizeof(int64_t) ) /* for appended length info */


/* reserve more than necessary at buf start to achieve page cache alignment */
const size_t READ_BUF_OFFSET =
   BEEGFS_MAX( (long)READ_BUF_OFFSET_PROTO_MIN, sysconf(_SC_PAGESIZE) );
/* reserve more than necessary at buf end to achieve page cache alignment */
const size_t READ_BUF_END_RESERVE =
   BEEGFS_MAX( (long)READ_BUF_END_PROTO_MIN, sysconf(_SC_PAGESIZE) );
/* read buffer size cutoff for protocol data */
const size_t READ_BUF_LEN_PROTOCOL_CUTOFF =
   READ_BUF_OFFSET + READ_BUF_END_RESERVE;


bool ReadLocalFileRDMAMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "ReadChunkFileRDMAMsg incoming";

   bool retVal = true; // return value

   int64_t readRes = 0;

   std::string fileHandleID(getFileHandleID() );
   bool isMirrorSession = isMsgHeaderFeatureFlagSet(READLOCALFILEMSG_FLAG_BUDDYMIRROR);

   // do session check only when it is not a mirror session
   bool useSessionCheck = isMirrorSession ? false :
      isMsgHeaderFeatureFlagSet(READLOCALFILEMSG_FLAG_SESSION_CHECK);

   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();
   auto session = sessions->referenceOrAddSession(getClientNumID());
   this->sessionLocalFiles = session->getLocalFiles();

   // select the right targetID

   uint16_t targetID = getTargetID();

   if(isMirrorSession )
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = isMsgHeaderFeatureFlagSet(READLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) ?
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
         sendError(ctx, -FhgfsOpsErr_COMMUNICATION);
         return true;
      }

      LOG(GENERAL, ERR, "Unknown target ID.", targetID);
      sendError(ctx, -FhgfsOpsErr_UNKNOWNTARGET);
      return true;
   }

   // check if we already have a session for this file...

   auto sessionLocalFile = sessionLocalFiles->referenceSession(
      fileHandleID, targetID, isMirrorSession);
   if(!sessionLocalFile)
   { // sessionLocalFile not exists yet => create, insert, re-get it
      if(useSessionCheck)
      { // server crashed during the write, maybe lost some data send error to client
         LogContext log("ReadChunkFileRDMAMsg incoming");
         log.log(Log_WARNING, "Potential cache loss for open file handle. (Server crash detected.) "
            "No session for file available. "
            "FileHandleID: " + fileHandleID);

         sendError(ctx, -FhgfsOpsErr_STORAGE_SRV_CRASHED);
         goto release_session;
      }

      std::string fileID = SessionTk::fileIDFromHandleID(fileHandleID);
      int openFlags = SessionTk::sysOpenFlagsFromFhgfsAccessFlags(getAccessFlags() );

      auto newFile = boost::make_unique<SessionLocalFile>(fileHandleID, targetID, fileID, openFlags,
         false);

      if(isMirrorSession)
         newFile->setIsMirrorSession(true);

      sessionLocalFile = sessionLocalFiles->addAndReferenceSession(std::move(newFile));
   }
   else
   { // session file exists
      if(useSessionCheck && sessionLocalFile->isServerCrashed() )
      { // server crashed during the write, maybe lost some data send error to client
         LogContext log("ReadChunkFileRDMAMsg incoming");
         log.log(Log_SPAM, "Potential cache loss for open file handle. (Server crash detected.) "
            "The session is marked as dirty. "
            "FileHandleID: " + fileHandleID);

         sendError(ctx, -FhgfsOpsErr_STORAGE_SRV_CRASHED);
         goto release_session;
      }
   }

   /* Note: the session file must be unlocked/released before we send the finalizing info,
       because otherwise we have a race when the client assumes the read is complete and tries
       to close the file (while the handle is actually still referenced on the server). */
   /* Note: we also must be careful to update the current offset before sending the final length
       info because otherwise the session file might have been released already and we have no
       longer access to the offset. */

   readRes = -1;
   try
   {
      // prepare file descriptor (if file not open yet then open it if it exists already)
      FhgfsOpsErr openRes = openFile(*target, sessionLocalFile.get());
      if(openRes != FhgfsOpsErr_SUCCESS)
      {
         sendError(ctx, -openRes);
         goto release_session;
      }

      // check if file exists
      if(!sessionLocalFile->getFD().valid())
      { // file didn't exist (not an error) => send EOF
         sendDone(ctx, 0);
         goto release_session;
      }

      // the actual read workhorse...

      readRes = incrementalReadStatefulAndSendRDMA(ctx, sessionLocalFile.get());

      LOG_DEBUG(logContext, Log_SPAM, "sending completed. "
         "readRes: " + StringTk::int64ToStr(readRes) );
      IGNORE_UNUSED_VARIABLE(readRes);

   }
   catch(SocketException& e)
   {
      LogContext(logContext).logErr(std::string("SocketException occurred: ") + e.what() );
      LogContext(logContext).log(Log_WARNING, "Details: "
         "sessionID: " + getClientNumID().str() + "; "
         "fileHandle: " + fileHandleID + "; "
         "offset: " + StringTk::int64ToStr(getOffset() ) + "; "
         "count: " + StringTk::int64ToStr(getCount() ) );

      sessionLocalFile->setOffset(-1); /* invalidate offset (we can only do this if still locked,
         but that's not a prob if we update offset correctly before send - see notes above) */

      retVal = false;
      goto release_session;
   }

release_session:

   // update operation counters

   if(likely(readRes > 0) )
      app->getNodeOpStats()->updateNodeOp(
         ctx.getSocket()->getPeerIP(), StorageOpCounter_READOPS, readRes, getMsgHeaderUserID() );

   return retVal;
}


/**
 * Note: This is similar to incrementalReadAndSend, but uses the offset from sessionLocalFile
 * to avoid calling seek every time.
 * Note: This is Version 2, it uses a preceding length info for each chunk.
 *
 * Warning: Do not use the returned value to set the new offset, as there might be other threads
 * that also did something with the file (i.e. the io-lock is released somewhere within this
 * method).
 *
 * @return number of bytes read or some arbitrary negative value otherwise
 */
int64_t ReadLocalFileRDMAMsgEx::incrementalReadStatefulAndSendRDMA(ResponseContext& ctx,
   SessionLocalFile* sessionLocalFile)
{
   /* note on protocol: this works by sending an int64 before each data chunk, which contains the
      length of the next data chunk; or a zero if no more data can be read; or a negative fhgfs
      error code in case of an error */

   /* note on session offset: the session offset must always be set before sending the data to the
      client (otherwise the client could send the next request before we updated the offset, which
      would lead to a race condition) */

   const char* logContext = "ReadChunkFileRDMAMsg (read incremental)";
   Config* cfg = Program::getApp()->getConfig();

   char* dataBuf = ctx.getBuffer();

   if (READ_BUF_LEN_PROTOCOL_CUTOFF >= ctx.getBufferLength())
   { // buffer too small. That shouldn't happen and is an error
      sendError(ctx, -FhgfsOpsErr_INTERNAL);
      return -1;
   }

   const ssize_t dataBufLen = ctx.getBufferLength();

   auto& fd = sessionLocalFile->getFD();
   int64_t oldOffset = sessionLocalFile->getOffset();
   int64_t newOffset = getOffset();

   bool skipReadAhead =
      unlikely(isMsgHeaderFeatureFlagSet(READLOCALFILEMSG_FLAG_DISABLE_IO) ||
      sessionLocalFile->getIsDirectIO());

   ssize_t readAheadSize = skipReadAhead ? 0 : cfg->getTuneFileReadAheadSize();
   ssize_t readAheadTriggerSize = cfg->getTuneFileReadAheadTriggerSize();

   if( (oldOffset < 0) || (oldOffset != newOffset) )
   {
      sessionLocalFile->resetReadCounter(); // reset sequential read counter
      sessionLocalFile->resetLastReadAheadTrigger();
   }
   else
   { // read continues at previous offset
      LOG_DEBUG(logContext, Log_SPAM,
         "fileID: " + sessionLocalFile->getFileID() + "; "
         "offset: " + StringTk::int64ToStr(getOffset() ) );
   }


   uint64_t toBeRead = getCount();
   size_t maxReadAtOnceLen = dataBufLen;

   // reduce maxReadAtOnceLen to achieve better read/send aync overlap
   /* (note: reducing makes only sense if we can rely on the kernel to do some read-ahead, so don't
      reduce for direct IO and for random IO) */
   if( (sessionLocalFile->getReadCounter() >= READ_USE_TUNEFILEREAD_TRIGGER) &&
       !sessionLocalFile->getIsDirectIO() )
      maxReadAtOnceLen = BEEGFS_MIN(dataBufLen, cfg->getTuneFileReadSize() );


   //
   // Start sending the data.  We will be writing directly into the remote buffers via RDMA.
   // In this protocol, we will RDMA the data and then only send a 'DONE' message with the
   // count of bytes written.
   //
   off_t readOffset = getOffset();
   RdmaInfo *rdma = this->getRdmaInfo();

   bool isFinal = (toBeRead == 0);
   size_t readLength = 0;
   ssize_t readRes = 0;
   ssize_t writeRes = 0;
   uint64_t remoteBuf;
   size_t remoteLength;
   uint64_t remoteOffset;
   uint64_t fileOffset = 0;

   if (!rdma->next(remoteBuf, remoteLength, remoteOffset))
   {
      LogContext(logContext).logErr("No entities in RDMA buffers.");
      return -FhgfsOpsErr_COMMUNICATION;
   }

   while (!isFinal)
   {
      readLength = BEEGFS_MIN(maxReadAtOnceLen, toBeRead);
      readLength = BEEGFS_MIN(readLength, remoteLength - remoteOffset);

      readRes = unlikely(isMsgHeaderFeatureFlagSet(READLOCALFILEMSG_FLAG_DISABLE_IO) ) ?
         readLength : MsgHelperIO::pread(*fd, dataBuf, readLength,  readOffset);

      if (unlikely(readRes == -1))
      {
         LogContext(logContext).log(Log_WARNING, "Unable to read file data. "
            "FileID: " + sessionLocalFile->getFileID() + "; "
            "SysErr: " + System::getErrString() );

         sessionLocalFile->setOffset(-1);
         sendError(ctx, -FhgfsOpsErr_INTERNAL);
         return -1;
      }

      //
      // Good read.  This is the final read if the remote buffer is full or
      // the file data read didn't fill the entire local buffer as requested.
      //
      toBeRead -= readRes;
      readOffset += readRes;
      fileOffset = getOffset() + getCount() - toBeRead;
      isFinal = (toBeRead == 0) || ((size_t)readRes < readLength);

      sessionLocalFile->setOffset(fileOffset); // update offset
      sessionLocalFile->incReadCounter(readRes); // update sequential read length

      ctx.getStats()->incVals.diskReadBytes += readRes; // update stats
      writeRes = writeData(ctx, dataBuf, (size_t)readRes, remoteBuf+remoteOffset, rdma->key);
      if (writeRes != readRes)
      {
         LogContext(logContext).log(Log_WARNING, "Unable to write file data to client. "
            "FileID: " + sessionLocalFile->getFileID() + "; "
            "SysErr: " + System::getErrString() );

         sessionLocalFile->setOffset(-1);
         sendError(ctx, -FhgfsOpsErr_COMMUNICATION);
         return -1;
      }

      if(!isFinal)
         checkAndStartReadAhead(sessionLocalFile, readAheadTriggerSize, fileOffset, readAheadSize);

      remoteOffset += readRes;
      if (toBeRead > 0 && remoteOffset == remoteLength)
      {
         if (!rdma->next(remoteBuf, remoteLength, remoteOffset))
         {
            LogContext(logContext).logErr("RDMA buffers expended but not all data sent. "
               "toBeRead: " + StringTk::uint64ToStr(toBeRead) + "; "
               "FileID: " + sessionLocalFile->getFileID() + "; ");
            sessionLocalFile->setOffset(-1);
            sendError(ctx, -FhgfsOpsErr_COMMUNICATION);
            return -1;
         }
      }
   } // end of while-loop

   sendDone(ctx, getCount() - toBeRead);
   return(getCount() - toBeRead);
}

/**
 * Starts read-ahead if enough sequential data has been read.
 *
 * Note: if getDisableIO() is true, we assume the caller sets readAheadSize==0, so getDisableIO()
 * is not checked explicitly within this function.
 *
 * @sessionLocalFile lastReadAheadOffset will be updated if read-head was triggered
 * @param readAheadTriggerSize the length of sequential IO that triggers read-ahead
 * @param currentOffset current file offset (where read-ahead would start)
 */
void ReadLocalFileRDMAMsgEx::checkAndStartReadAhead(SessionLocalFile* sessionLocalFile,
   ssize_t readAheadTriggerSize, off_t currentOffset, off_t readAheadSize)
{
   const char* logContext = "ReadChunkFileRDMAMsg (read-ahead)";

   if(!readAheadSize)
      return;

   int64_t readCounter = sessionLocalFile->getReadCounter();
   int64_t nextReadAheadTrigger = sessionLocalFile->getLastReadAheadTrigger() ?
      sessionLocalFile->getLastReadAheadTrigger() + readAheadSize : readAheadTriggerSize;

   if(readCounter < nextReadAheadTrigger)
      return; // we're not at the trigger point yet

   /* start read-head...
      (read-ahead is supposed to be non-blocking if there are free slots in the device IO queue) */

   LOG_DEBUG(logContext, Log_SPAM,
      std::string("Starting read-ahead... ") +
      "offset: " + StringTk::int64ToStr(currentOffset) + "; "
      "size: " + StringTk::int64ToStr(readAheadSize) );
   IGNORE_UNUSED_VARIABLE(logContext);

   MsgHelperIO::readAhead(*sessionLocalFile->getFD(), currentOffset, readAheadSize);

   // update trigger

   sessionLocalFile->setLastReadAheadTrigger(readCounter);
}


/**
 * Open the file if a filedescriptor is not already set in sessionLocalFile.
 * If the file needs to be opened, this method will check the target consistency state before
 * opening.
 *
 * @return we return the special value FhgfsOpsErr_COMMUNICATION here in some cases to indirectly
 * ask the client for a retry (e.g. if target consistency is not good for buddymirrored chunks).
 */
FhgfsOpsErr ReadLocalFileRDMAMsgEx::openFile(const StorageTarget& target,
      SessionLocalFile* sessionLocalFile)
{
   const char* logContext = "ReadChunkFileRDMAMsg (open)";

   bool isBuddyMirrorChunk = sessionLocalFile->getIsMirrorSession();


   if (sessionLocalFile->getFD().valid())
      return FhgfsOpsErr_SUCCESS; // file already open => nothing to be done here


   // file not open yet => get targetFD and check consistency state

   const auto consistencyState = target.getConsistencyState();
   const int targetFD = isBuddyMirrorChunk ? *target.getMirrorFD() : *target.getChunkFD();

   if(unlikely(consistencyState != TargetConsistencyState_GOOD) && isBuddyMirrorChunk)
   { // this is a request for a buddymirrored chunk on a non-good target
      LogContext(logContext).log(Log_NOTICE, "Refusing request. Target consistency is not good. "
         "targetID: " + StringTk::uintToStr(target.getID()));

      return FhgfsOpsErr_COMMUNICATION;
   }

   FhgfsOpsErr openChunkRes = sessionLocalFile->openFile(targetFD, getPathInfo(), false, NULL);

   return openChunkRes;
}
#endif /* BEEGFS_NVFS */

