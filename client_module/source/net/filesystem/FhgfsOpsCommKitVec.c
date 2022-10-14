#include <app/App.h>
#include <common/net/message/session/rw/ReadLocalFileV2Msg.h>
#include <common/net/message/session/rw/WriteLocalFileMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/FhgfsTypes.h>
#include <common/toolkit/MessagingTk.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/Thread.h>
#include <filesystem/FhgfsInode.h>
#include <filesystem/FhgfsOpsPages.h>

#define FhgfsOpsCommKitVec
#include "FhgfsOpsCommKitVec.h"


/**
 * Prepare a read request
 *
 * @return false on error (means 'break' for the surrounding switch statement)
 */
void __FhgfsOpsCommKitVec_readfileStagePREPARE(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   Config* cfg = App_getConfig(commHelper->app);
   TargetMapper* targetMapper = App_getTargetMapper(commHelper->app);
   NodeStoreEx* storageNodes = App_getStorageNodes(commHelper->app);
   Node* localNode = App_getLocalNode(commHelper->app);
   const NumNodeID localNodeNumID = Node_getNumID(localNode);

   FhgfsOpsErr resolveErr;
   NodeConnPool* connPool;
   ReadLocalFileV2Msg readMsg;
   ssize_t sendRes;
   bool needCleanup = false;
   bool isSocketException = false;

   uint16_t nodeReferenceTargetID = comm->targetID; /* to avoid overriding original targetID
      in case of buddy-mirroring, because we need the mirror group ID for the msg and for retries */

   loff_t offset = FhgfsOpsCommKitVec_getOffset(comm);

   comm->read.reqLen = FhgfsOpsCommKitVec_getRemainingDataSize(comm);

   comm->sock = NULL;
   comm->nodeResult = -FhgfsOpsErr_COMMUNICATION;

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   // select the right targetID

   if(StripePattern_getPatternType(commHelper->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror)
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = App_getStorageBuddyGroupMapper(commHelper->app);

      nodeReferenceTargetID = comm->useBuddyMirrorSecond ?
         MirrorBuddyGroupMapper_getSecondaryTargetID(mirrorBuddies, comm->targetID) :
         MirrorBuddyGroupMapper_getPrimaryTargetID(mirrorBuddies, comm->targetID);

      // note: only log message here, error handling below through invalid node reference
      if(unlikely(!nodeReferenceTargetID) )
         Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
            "Invalid mirror buddy group ID: %hu", comm->targetID);
   }

   // get the target-node reference
   comm->node = NodeStoreEx_referenceNodeByTargetID(storageNodes,
      nodeReferenceTargetID, targetMapper, &resolveErr);
   if(unlikely(!comm->node) )
   { // unable to resolve targetID
      comm->nodeResult = -resolveErr;

      needCleanup = true;
      goto outErr;
   }

   connPool = Node_getConnPool(comm->node);

   // connect
   comm->sock = NodeConnPool_acquireStreamSocketEx(connPool, true);
   if(!comm->sock)
   {  // connection error
      if (fatal_signal_pending(current))
      {
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_DEBUG,
            commHelper->logContext, "Connect to server canceled by pending signal: %s",
            Node_getNodeIDWithTypeStr(comm->node) );

         comm->nodeResult = FhgfsOpsErr_INTERRUPTED;
      }
      else
      {
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING,
            commHelper->logContext, "Unable to connect to server: %s",
            Node_getNodeIDWithTypeStr(comm->node) );

      }

      needCleanup = true;
      goto outErr;
   }

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_SPAM, __func__,
      "Acquire read node: %s offset: %lli size: %zu",
      Node_getNodeIDWithTypeStr(comm->node), offset, comm->read.reqLen);

   // prepare message
   ReadLocalFileV2Msg_initFromSession(&readMsg, localNodeNumID,
      commHelper->ioInfo->fileHandleID, comm->targetID, commHelper->ioInfo->pathInfo,
      commHelper->ioInfo->accessFlags, offset, comm->read.reqLen);

   NetMessage_setMsgHeaderTargetID( (NetMessage*)&readMsg, nodeReferenceTargetID);

   if (comm->firstWriteDoneForTarget && Config_getSysSessionChecksEnabled(cfg))
      NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&readMsg,
         READLOCALFILEMSG_FLAG_SESSION_CHECK);

   if(StripePattern_getPatternType(commHelper->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror)
   {
      NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&readMsg, READLOCALFILEMSG_FLAG_BUDDYMIRROR);

      if(comm->useBuddyMirrorSecond)
         NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&readMsg,
            READLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);
   }

   if(App_getNetBenchModeEnabled(commHelper->app) )
      NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&readMsg, READLOCALFILEMSG_FLAG_DISABLE_IO);

   NetMessage_serialize( (NetMessage*)&readMsg, comm->msgBuf, BEEGFS_COMMKIT_MSGBUF_SIZE);

   comm->hdrLen = NetMessage_getMsgLength( (NetMessage*)&readMsg);

   comm->nodeResult = 0; // ready to read, so set this variable to 0

   sendRes = Socket_send(comm->sock, comm->msgBuf, comm->hdrLen, 0);

#if (BEEGFS_COMMKIT_DEBUG & COMMKIT_DEBUG_READ_SEND )
   if (sendRes > 0 && jiffies % CommKitErrorInjectRate == 0)
   {
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, __func__,
         "Injecting timeout error after (successfully) sending data.");
      sendRes = -ETIMEDOUT;
   }
#endif

   if(unlikely(sendRes <= 0) )
   {
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, commHelper->logContext,
         "Failed to send message to %s: %s", Node_getNodeIDWithTypeStr(comm->node),
         Socket_getPeername(comm->sock) );

      isSocketException = true;
      goto outErr;
   }

   __FhgfsOpsCommKitVec_readfileStageRECVHEADER(commHelper, comm);
   return;

outErr:
   if (needCleanup)
      __FhgfsOpsCommKitVec_readfileStageCLEANUP(commHelper, comm);

   if (isSocketException)
      __FhgfsOpsCommKitVec_readfileStageRECVHEADER(commHelper, comm);
}

/**
 * Receive how many data the server is going to send next, this is also called after each
 * *complete* read request
 */
void __FhgfsOpsCommKitVec_readfileStageRECVHEADER(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   ssize_t recvRes;
   char dataLenBuf[sizeof(int64_t)]; // length info in fhgfs network byte order
   int64_t lengthInfo; // length info in fhgfs host byte order
   DeserializeCtx ctx = { dataLenBuf, sizeof(int64_t) };

   bool isSocketException = false;
   bool needCleanup = false;

   Config* cfg = App_getConfig(commHelper->app);

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   recvRes = Socket_recvExactT(comm->sock, &dataLenBuf, sizeof(int64_t), 0, cfg->connMsgLongTimeout);

   if(unlikely(recvRes <= 0) )
   { // error
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, commHelper->logContext,
         "Failed to receive length info from %s: %s",
         Node_getNodeIDWithTypeStr(comm->node), Socket_getPeername(comm->sock) );

      isSocketException = true;
      goto outErr;
   }

   // got the length info response
   Serialization_deserializeInt64(&ctx, &lengthInfo);

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, commHelper->logContext,
      "Received lengthInfo from %s: %lld", Node_getID(comm->node), (long long)lengthInfo);

#if (BEEGFS_COMMKIT_DEBUG & COMMKIT_DEBUG_READ_HEADER)
   if (recvRes > 0 && jiffies % CommKitErrorInjectRate == 0)
   {
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, __func__,
         "Injecting timeout error after (successfully) receiving data.");

      isSocketException = true;
      goto outErr;
   }
#endif

   if(lengthInfo <= 0)
   { // end of file data transmission

      if(unlikely(lengthInfo < 0) )
      {
         comm->nodeResult = lengthInfo; // error occurred

         isSocketException = true;
      }
      else
      {  // EOF
         needCleanup = true;
      }

      goto outErr;
   }

   if (unlikely(lengthInfo > comm->read.reqLen) )
   {
      Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
         "Bug: Server wants to send more than we requested (%zu vs. %llu)",
         comm->read.reqLen, lengthInfo);

      isSocketException = true;
      goto outErr;
   }

   // positive result => node is going to send some file data
   comm->read.serverSize = lengthInfo;

   __FhgfsOpsCommKitVec_readfileStageRECVDATA(commHelper, comm);
   return;

outErr:
   if (needCleanup)
      __FhgfsOpsCommKitVec_readfileStageCLEANUP(commHelper, comm);

   if (isSocketException)
      __FhgfsOpsCommKitVec_readfileStageSOCKETEXCEPTION(commHelper, comm);
}



/**
 * Read the data from the server
 */
void __FhgfsOpsCommKitVec_readfileStageRECVDATA(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   size_t receiveSum = 0; // sum of recvRes
   FhgfsPage *fhgfsPage;
   size_t pageLen;   // length of a page
   ssize_t recvRes;

   Config* cfg = App_getConfig(commHelper->app);

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   do
   {  // walk over all pages of this state up to the size the server promise to give us
      FhgfsChunkPageVec* pageVec = comm->pageVec;
      void* pageDataPtr;
      size_t requestLength;

#ifdef BEEGFS_DEBUG
      size_t vecIdx = FhgfsChunkPageVec_getCurrentListVecIterIdx(pageVec);
      loff_t offset = FhgfsOpsCommKitVec_getOffset(comm);
#endif

      fhgfsPage = FhgfsChunkPageVec_iterateGetNextPage(pageVec);

      if (unlikely(!fhgfsPage) )
      {
         Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
            "BUG: Iterator does not have more pages!" );

         comm->nodeResult = -FhgfsOpsErr_INTERNAL;
         goto outSocketException;
      }

      LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_SPAM, __func__,
         "vecSize: %zu offset: %lld; Vec-Idx: %zu receivedData: %ld",
         FhgfsChunkPageVec_getDataSize(pageVec), offset,
         vecIdx, receiveSum);

      pageLen = fhgfsPage->length;
      pageDataPtr = fhgfsPage->data;

      requestLength = MIN(pageLen, comm->read.serverSize - receiveSum);

      // receive available dataPart
      recvRes = Socket_recvExactT(comm->sock, pageDataPtr, requestLength, 0, cfg->connMsgLongTimeout);

      LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__,
         "requested: %lld; received: %lld",
         (long long)requestLength, (long long)recvRes);

#if (BEEGFS_COMMKIT_DEBUG & COMMKIT_DEBUG_RECV_DATA)
   if (recvRes > 0 && jiffies % CommKitErrorInjectRate == 0)
   {
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, __func__,
         "Injecting timeout error after (successfully) receiving data.");
      recvRes = -ETIMEDOUT;
   }
#endif

      if(unlikely(recvRes <= 0) )
      { // something went wrong
         goto outReadErr;
      }

      receiveSum += recvRes;

      if (unlikely(requestLength < pageLen && receiveSum < comm->read.serverSize) )
      {
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING,
            commHelper->logContext, "Bug/Warning: requestLength < pageLength (%zu, %zu)",
            requestLength, pageLen);

         goto outReadErr;
      }

      comm->numSuccessPages++;

   } while (receiveSum < comm->read.serverSize);

   // all data for this state and round received, add it up
   comm->nodeResult += receiveSum;

   // all of the data has been received => receive the next lengthInfo in the header stage
   comm->doHeader = true;

   return;

outSocketException:
   __FhgfsOpsCommKitVec_readfileStageSOCKETEXCEPTION(commHelper, comm);
   return;

outReadErr:
   __FhgfsOpsCommKitVec_handleReadError(commHelper, comm, fhgfsPage, recvRes, pageLen);
      return;
}

/**
 * Take the right action based on the error type
 */
void __FhgfsOpsCommKitVec_handleReadError(CommKitVecHelper* commHelper, FhgfsCommKitVec* comm,
   FhgfsPage* fhgfsPage, ssize_t recvRes, size_t missingPgLen)
{
   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   if(recvRes == -EFAULT)
   { // bad buffer address given
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, commHelper->logContext,
         "Bad buffer address");

      comm->nodeResult = -FhgfsOpsErr_ADDRESSFAULT;
   }
   else
   {
      if (recvRes > 0)
      {  /* requestLength < pageLength && comm->read.receivedLength < comm->read.serverSize */
         comm->nodeResult = -FhgfsOpsErr_INTERNAL;
      }
      if(recvRes == -ETIMEDOUT)
      { // timeout
         Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
            "Communication timeout in RECVDATA stage. Node: %s",
            Node_getNodeIDWithTypeStr(comm->node) );
         comm->nodeResult = -FhgfsOpsErr_COMMUNICATION;
      }
      else
      { // error
         Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
            "Communication error in RECVDATA stage. Node: %s (recv result: %lld)",
            Node_getNodeIDWithTypeStr(comm->node), (long long)recvRes);

         comm->nodeResult = -FhgfsOpsErr_INTERNAL;
      }

      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_SPAM, commHelper->logContext,
         "Request details: missingLength of %s: %lld", Node_getNodeIDWithTypeStr(comm->node),
         (long long)missingPgLen);
   }

   __FhgfsOpsCommKitVec_readfileStageSOCKETEXCEPTION(commHelper, comm);
}


/**
 * Socket exception happened
 */
void __FhgfsOpsCommKitVec_readfileStageSOCKETEXCEPTION(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   const char* logCommErrTxt = "Communication error";

   size_t remainingDataSize = FhgfsOpsCommKitVec_getRemainingDataSize(comm);
   loff_t offset            = FhgfsOpsCommKitVec_getOffset(comm);

   Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, commHelper->logContext,
      "%s: Sent request: node: %s; fileHandleID: %s; offset: %lld; size: %zd",
      logCommErrTxt, Node_getNodeIDWithTypeStr(comm->node), commHelper->ioInfo->fileHandleID,
      (long long) offset, remainingDataSize );

   NodeConnPool_invalidateStreamSocket(Node_getConnPool(comm->node), comm->sock);
   comm->sock = NULL;

   if (comm->nodeResult >= 0)
   {  // make sure not to overwrite already assigned error codes
      comm->nodeResult = -FhgfsOpsErr_COMMUNICATION;
   }

   __FhgfsOpsCommKitVec_readfileStageCLEANUP(commHelper, comm);
}


/**
 * Clean up the current state after an error or too few data.
 */
void __FhgfsOpsCommKitVec_readfileStageCLEANUP(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   // actual clean-up

   if (comm->sock)
   {
      LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_SPAM, __func__,
         "Release socket, read node: %s", Node_getNodeIDWithTypeStr(comm->node) );

      NodeConnPool_releaseStreamSocket(Node_getConnPool(comm->node), comm->sock);
      comm->sock = NULL;
   }

   if(likely(comm->node) )
      Node_put(comm->node);

   // prepare next stage

}

/**
 * Handle all remaining pages of this state.
 * Note: Must be always called - on read success and read-error!
 */
void __FhgfsOpsCommKitVec_readfileStageHandlePages(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   // const char* logContext = "CommKitVec_readfileStageHandlePages";
   FhgfsChunkPageVec* fhgfsPageVec = comm->pageVec;
   struct inode* inode = FhgfsChunkPageVec_getInode(fhgfsPageVec);

   bool needInodeRefresh = true;
   FhgfsPage* fhgfsPage;
   size_t pgCount = 0;
   int64_t readRes = comm->nodeResult;

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__,
      "nodeResult: %lli (%s)", comm->nodeResult,
      (comm->nodeResult < 0) ? FhgfsOpsErr_toErrString(-comm->nodeResult):
         FhgfsOpsErr_toErrString(FhgfsOpsErr_SUCCESS));

   if (unlikely(comm->nodeResult == -FhgfsOpsErr_COMMUNICATION ||
                comm->nodeResult == -FhgfsOpsErr_AGAIN))
   {
      // don't do anything, IO can be possibly re-tried
      goto out;
   }

   FhgfsChunkPageVec_resetIterator(fhgfsPageVec);

   // walk over all successfully read pages
   while (pgCount < comm->numSuccessPages && readRes > 0)
   {
      fhgfsPage = FhgfsChunkPageVec_iterateGetNextPage(fhgfsPageVec);
      if (!fhgfsPage)
         break;

      FhgfsOpsPages_endReadPage(commHelper->log, inode, fhgfsPage, readRes);

      readRes -= PAGE_SIZE;
      pgCount++;
   }

   if (unlikely(readRes > 0)) {
      Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
         "%s: Bug: readRes too large!", __func__);
      comm->nodeResult = -FhgfsOpsErr_INTERNAL;
   }

   // walk over the remaining pages
   while (1)
   {
      bool isShortRead;
      int pageReadRes = (comm->nodeResult >= 0) ? comm->nodeResult : -1;

      fhgfsPage = FhgfsChunkPageVec_iterateGetNextPage(fhgfsPageVec);
      if (!fhgfsPage)
         break;

      if (pageReadRes >= 0)
      {
         isShortRead = FhgfsOpsPages_isShortRead(inode, FhgfsPage_getPageIndex(fhgfsPage),
            needInodeRefresh);

         needInodeRefresh = false; // a single inode refresh is sufficient

      LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_SPAM, __func__,
         "nodeResult: %lld pageIdx: %ld is-short-read: %d",
         comm->nodeResult, FhgfsPage_getPageIndex(fhgfsPage), isShortRead);

         if (isShortRead)
         {  // file size includes this page, but we don't have data, so zero it
            FhgfsPage_zeroPage(fhgfsPage);
         }
         else
         {  // the file is smaller than this page, nothing to be done
         }

      }

      FhgfsOpsPages_endReadPage(commHelper->log, inode, fhgfsPage, pageReadRes);
   }

out:
   return;
}


/**
 * @return false on error (means 'break' for the surrounding switch statement)
 */
void __FhgfsOpsCommKitVec_writefileStagePREPARE(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   Config* cfg = App_getConfig(commHelper->app);
   TargetMapper* targetMapper = App_getTargetMapper(commHelper->app);
   NodeStoreEx* storageNodes = App_getStorageNodes(commHelper->app);
   Node* localNode = App_getLocalNode(commHelper->app);
   const NumNodeID localNodeNumID = Node_getNumID(localNode);

   FhgfsOpsErr resolveErr;
   NodeConnPool* connPool;
   WriteLocalFileMsg writeMsg;

   uint16_t nodeReferenceTargetID = comm->targetID; /* to avoid overriding original targetID
      in case of buddy-mirroring, because we need the mirror group ID for the msg and for retries */

   size_t remainingDataSize = FhgfsOpsCommKitVec_getRemainingDataSize(comm);
   loff_t offset            = FhgfsOpsCommKitVec_getOffset(comm);

   comm->sock = NULL;
   comm->nodeResult = -FhgfsOpsErr_COMMUNICATION;

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   if(StripePattern_getPatternType(commHelper->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror)
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = App_getStorageBuddyGroupMapper(commHelper->app);

      nodeReferenceTargetID = comm->useBuddyMirrorSecond ?
         MirrorBuddyGroupMapper_getSecondaryTargetID(mirrorBuddies, comm->targetID) :
         MirrorBuddyGroupMapper_getPrimaryTargetID(mirrorBuddies, comm->targetID);

      // note: only log message here, error handling below through invalid node reference
      if(unlikely(!nodeReferenceTargetID) )
         Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
            "Invalid mirror buddy group ID: %hu", comm->targetID);
   }

   // get the target-node reference
   comm->node = NodeStoreEx_referenceNodeByTargetID(storageNodes,
      nodeReferenceTargetID, targetMapper, &resolveErr);
   if(unlikely(!comm->node) )
   { // unable to resolve targetID
      comm->nodeResult = -resolveErr;

      goto outErr;
   }

   connPool = Node_getConnPool(comm->node);

   // connect
   comm->sock = NodeConnPool_acquireStreamSocketEx(connPool, true);
   if(!comm->sock)
   {  // connection error
      if (fatal_signal_pending(current))
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_DEBUG,
            commHelper->logContext, "Connect to server canceled by pending signal: %s",
            Node_getNodeIDWithTypeStr(comm->node) );
      else
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING,
            commHelper->logContext, "Unable to connect to server: %s",
            Node_getNodeIDWithTypeStr(comm->node) );

      goto outErr;
   }

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_SPAM, __func__,
      "Acquire write state: node: %s offset: %lld size: %zd",
      Node_getNodeIDWithTypeStr(comm->node), (long long) offset, remainingDataSize);

   // prepare message
   WriteLocalFileMsg_initFromSession(&writeMsg, localNodeNumID,
      commHelper->ioInfo->fileHandleID, comm->targetID, commHelper->ioInfo->pathInfo,
      commHelper->ioInfo->accessFlags, offset, remainingDataSize);

   NetMessage_setMsgHeaderTargetID( (NetMessage*)&writeMsg, nodeReferenceTargetID);

   if (Config_getQuotaEnabled(cfg) )
      WriteLocalFileMsg_setUserdataForQuota(&writeMsg, commHelper->ioInfo->userID,
         commHelper->ioInfo->groupID);

   if (comm->firstWriteDoneForTarget && Config_getSysSessionChecksEnabled(cfg))
      NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&writeMsg,
         WRITELOCALFILEMSG_FLAG_SESSION_CHECK);

   if(StripePattern_getPatternType(commHelper->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror)
   {
      NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&writeMsg,
         WRITELOCALFILEMSG_FLAG_BUDDYMIRROR);

      if(comm->useServersideMirroring)
         NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&writeMsg,
            WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_FORWARD);

      if(comm->useBuddyMirrorSecond)
         NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&writeMsg,
            WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);
   }

   if(App_getNetBenchModeEnabled(commHelper->app) )
      NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&writeMsg,
         WRITELOCALFILEMSG_FLAG_DISABLE_IO);

   NetMessage_serialize( (NetMessage*)&writeMsg, comm->msgBuf, BEEGFS_COMMKIT_MSGBUF_SIZE);

   comm->hdrLen = NetMessage_getMsgLength( (NetMessage*)&writeMsg);

   comm->doHeader = true;

   return;

outErr:
   __FhgfsOpsCommKitVec_writefileStageCLEANUP(commHelper, comm);
}

/**
 * Tell the server how many data we are going to send
 */
void __FhgfsOpsCommKitVec_writefileStageSENDHEADER(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   ssize_t sendRes;

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   sendRes = Socket_send(comm->sock, comm->msgBuf, comm->hdrLen, 0);

#if (BEEGFS_COMMKIT_DEBUG & COMMKIT_DEBUG_WRITE_HEADER )
      if (sendRes == FhgfsOpsErr_SUCCESS && jiffies % CommKitErrorInjectRate == 0)
      {
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, __func__,
            "Injecting timeout error after (successfully) sending data.");
         sendRes = -ETIMEDOUT;
      }
#endif

   if(unlikely(sendRes <= 0) )
   {
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, commHelper->logContext,
         "Failed to send message to %s: %s", Node_getNodeIDWithTypeStr(comm->node),
         Socket_getPeername(comm->sock) );

      goto outErr;
   }

   __FhgfsOpsCommKitVec_writefileStageSENDDATA(commHelper, comm);

   return;

outErr:
   __FhgfsOpsCommKitVec_writefileStageSOCKETEXCEPTION(commHelper, comm);
}

/**
 * Send the data (of our current state)
 *
 * @param state  - current state to send data for
 */
void __FhgfsOpsCommKitVec_writefileStageSENDDATA(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   ssize_t sendDataPartRes;
   FhgfsChunkPageVec *pageVec = comm->pageVec;

   FhgfsPage *fhgfsPage;

   size_t vecIdx = FhgfsChunkPageVec_getCurrentListVecIterIdx(pageVec);

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   {
      loff_t offset = FhgfsOpsCommKitVec_getOffset(comm);

      LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_SPAM, __func__,
         "targetID: %hu offset: %lld vecSize: %u, current-pgIdx: %zu",
         comm->targetID, (long long) offset, FhgfsChunkPageVec_getSize(pageVec), vecIdx);
      IGNORE_UNUSED_VARIABLE(offset);
   }

   // walk over all pages
   while (true)
   {
      void* data;

      size_t dataLength;

      vecIdx = FhgfsChunkPageVec_getCurrentListVecIterIdx(pageVec);

      fhgfsPage = FhgfsChunkPageVec_iterateGetNextPage(pageVec);
      if (!fhgfsPage)
         break;

      dataLength = fhgfsPage->length;
      data = fhgfsPage->data;

      // send dataPart blocking
      sendDataPartRes = Socket_send(comm->sock, data, dataLength, 0);

      LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__,
          "VecIdx: %zu; size: %lld; PgLen: %d, sendRes: %zd",
          vecIdx, (long long)dataLength, fhgfsPage->length, sendDataPartRes);

#if (BEEGFS_COMMKIT_DEBUG & COMMKIT_DEBUG_WRITE_SEND)
      if (sendDataPartRes > 0 && jiffies % CommKitErrorInjectRate == 0)
      {
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, __func__,
            "Injecting timeout error after (successfully) sending data.");
         sendDataPartRes = -ETIMEDOUT;
      }
#endif

      if (unlikely(sendDataPartRes <= 0) )
      {
         if(sendDataPartRes == -EFAULT)
         { // bad buffer address given
            Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_DEBUG,
               commHelper->logContext, "Bad buffer address");

            comm->nodeResult = -FhgfsOpsErr_ADDRESSFAULT;

            goto outErr;
         }

         Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
            "Communication error in SENDDATA stage. "
            "Node: %s; Request details: %zu/%u pages",
            Node_getNodeIDWithTypeStr(comm->node), vecIdx, FhgfsChunkPageVec_getSize(pageVec) );


         goto outErr;
      }

      if (sendDataPartRes < dataLength)
      {
         Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
            "Error/Bug: Node: %s; Less data sent than requested; Request details: %zu/%u pages",
            Node_getNodeIDWithTypeStr(comm->node), vecIdx, FhgfsChunkPageVec_getSize(pageVec) );

         goto outErr;
      }
   }

   __FhgfsOpsCommKitVec_writefileStageRECV(commHelper, comm);
   return;

outErr:
   __FhgfsOpsCommKitVec_writefileStageSOCKETEXCEPTION(commHelper, comm);
}


/**
 * Ask the server side for errors
 */
void __FhgfsOpsCommKitVec_writefileStageRECV(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   ssize_t respRes;
   bool isSocketException = false;

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   respRes = MessagingTk_recvMsgBuf(commHelper->app, comm->sock,
      comm->msgBuf, BEEGFS_COMMKIT_MSGBUF_SIZE);

#if (BEEGFS_COMMKIT_DEBUG & COMMKIT_DEBUG_WRITE_RECV)
      if (respRes == FhgfsOpsErr_SUCCESS && jiffies % CommKitErrorInjectRate == 0)
      {
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING, __func__,
            "Injecting timeout error after (successfully) sending data.");
         respRes = -ETIMEDOUT;
      }
#endif

   if(unlikely(respRes <= 0) )
   { // receive failed
      // note: signal pending log msg will be printed in stage SOCKETEXCEPTION, so no need here
      if (!fatal_signal_pending(current))
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING,
            commHelper->logContext,
            "Receive failed from: %s @ %s", Node_getNodeIDWithTypeStr(comm->node),
            Socket_getPeername(comm->sock) );

      isSocketException = true;
      goto outErr;
   }
   else
   { // got response => deserialize it
      bool deserRes;

      WriteLocalFileRespMsg_init(&comm->write.respMsg);

      deserRes = NetMessageFactory_deserializeFromBuf(commHelper->app, comm->msgBuf, respRes,
         (NetMessage*)&comm->write.respMsg, NETMSGTYPE_WriteLocalFileResp);

      if(unlikely(!deserRes) )
      { // response invalid
         Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_WARNING,
            commHelper->logContext,
            "Received invalid response from %s. Expected type: %d. Disconnecting: %s",
            Node_getNodeIDWithTypeStr(comm->node), NETMSGTYPE_WriteLocalFileResp,
            Socket_getPeername(comm->sock) );

         isSocketException = true;
         goto outErr;
      }
      else
      { // correct response
         int64_t writeRespValue = WriteLocalFileRespMsg_getValue(&comm->write.respMsg);

         LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, 5, __func__,
            "writeRespValue: %lld", (long long) writeRespValue);

         comm->nodeResult = writeRespValue;
      }
   }

   __FhgfsOpsCommKitVec_writefileStageCLEANUP(commHelper, comm);
   return;

outErr:
   if (isSocketException)
      __FhgfsOpsCommKitVec_writefileStageSOCKETEXCEPTION(commHelper, comm);

}

/**
 * Socket exception happend
 */
void __FhgfsOpsCommKitVec_writefileStageSOCKETEXCEPTION(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   const char* logCommInterruptedTxt = "Communication interrupted by signal";
   const char* logCommErrTxt = "Communication error";

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   if (fatal_signal_pending(current))
   { // interrupted by signal
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_NOTICE, commHelper->logContext,
         "%s. Node: %s", logCommInterruptedTxt, Node_getNodeIDWithTypeStr(comm->node) );
   }
   else
   { // "normal" connection error
      size_t remainingDataSize = FhgfsOpsCommKitVec_getRemainingDataSize(comm);
      loff_t offset            = FhgfsOpsCommKitVec_getOffset(comm);

      Logger_logTopErrFormatted(commHelper->log, LogTopic_COMMKIT, commHelper->logContext,
         "%s. Node: %s", logCommErrTxt, Node_getNodeIDWithTypeStr(comm->node) );

      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, commHelper->logContext,
         "Sent request: node: %s; fileHandleID: %s; offset: %lld; size: %zu",
         Node_getNodeIDWithTypeStr(comm->node), commHelper->ioInfo->fileHandleID,
         (long long)offset, remainingDataSize);
   }

   if (comm->nodeResult >= 0)
   {  // make sure not to overwrite already assigned error codes
      comm->nodeResult = -FhgfsOpsErr_COMMUNICATION;
   }

   NodeConnPool_invalidateStreamSocket(Node_getConnPool(comm->node), comm->sock);
   comm->sock = NULL;

   __FhgfsOpsCommKitVec_writefileStageCLEANUP(commHelper, comm);
}

/**
 * Clean up a connection, if there was an error
 */
void __FhgfsOpsCommKitVec_writefileStageCLEANUP(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   // actual clean-up

   if (comm->sock)
   {
      LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, 5, __func__,
         "Release write node: %s", Node_getNodeIDWithTypeStr(comm->node) );

      NodeConnPool_releaseStreamSocket(Node_getConnPool(comm->node), comm->sock);
      comm->sock = NULL;
   }

   if(likely(comm->node) )
      Node_put(comm->node);

   // prepare next stage
}

/**
 * Handle all remaining pages of this state.
 * Note: Must be always called - on read success and read-error!
 */
void __FhgfsOpsCommKitVec_writefileStageHandlePages(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   FhgfsChunkPageVec *pageVec = comm->pageVec;

   struct inode* inode = FhgfsChunkPageVec_getInode(pageVec);

   FhgfsPage* lastInProgressPage = NULL;
   FhgfsPage* firstUnhandledPage;

   ssize_t remainingServerWriteRes = comm->nodeResult;

   LOG_DEBUG_TOP_FORMATTED(commHelper->log, LogTopic_COMMKIT, Log_DEBUG, __func__, "enter");

   if (unlikely(comm->nodeResult == -FhgfsOpsErr_AGAIN ||
                comm->nodeResult == -FhgfsOpsErr_COMMUNICATION) )
      return; // try again

   firstUnhandledPage = FhgfsChunkPageVec_iterateGetNextPage(pageVec); // might be NULL

   FhgfsChunkPageVec_resetIterator(pageVec);

   // walk over all pages in the page list vec
   while(1)
   {
      int writeRes;

      FhgfsPage* fhgfsPage = FhgfsChunkPageVec_iterateGetNextPage(pageVec);
      if (!fhgfsPage)
         break;

      if (fhgfsPage == lastInProgressPage)
         continue; // we already handled it above

      if (remainingServerWriteRes > 0)
      {
         writeRes = fhgfsPage->length;
         remainingServerWriteRes -= fhgfsPage->length;
      }
      else
         writeRes = -EREMOTEIO;

      if (unlikely(fhgfsPage == firstUnhandledPage && remainingServerWriteRes > 0) )
      {
         printk_fhgfs(KERN_INFO,
            "Bug: Server claims to have written more pages than we counted internally!");
         writeRes = -EIO;
      }

      if (unlikely(writeRes < 0) )
      {
         FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

         spin_lock(&inode->i_lock);
         FhgfsInode_setWritePageError(fhgfsInode);
         spin_unlock(&inode->i_lock);
      }

      FhgfsOpsPages_endWritePage(fhgfsPage->page, writeRes, inode);
   }

   if (unlikely(remainingServerWriteRes < 0) )
      Logger_logTopFormatted(commHelper->log, LogTopic_COMMKIT, Log_ERR, __func__,
         "Write error detected (check server logs for details)!");
}

/**
 * Start the remote read IO
 */
static int64_t __FhgfsOpsCommKitVec_readfileCommunicate(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   comm->doHeader = false;

   __FhgfsOpsCommKitVec_readfileStagePREPARE(commHelper, comm);
   while (comm->doHeader)
   {
      comm->doHeader = false;

      // set doHeader to true if required
      __FhgfsOpsCommKitVec_readfileStageRECVHEADER(commHelper, comm);
   }

   return comm->nodeResult;
}

/**
 * Start the remote write IO
 */
static int64_t __FhgfsOpsCommKitVec_writefileCommunicate(CommKitVecHelper* commHelper,
   FhgfsCommKitVec* comm)
{
   FhgfsInode* inode = BEEGFS_INODE(comm->pageVec->inode);

   FhgfsInode_incWriteBackCounter(inode);

   comm->doHeader = false;

   __FhgfsOpsCommKitVec_writefileStagePREPARE(commHelper, comm);
   while (comm->doHeader)
   {
      comm->doHeader = false;

      // set doHeader to true if required
      __FhgfsOpsCommKitVec_writefileStageSENDHEADER(commHelper, comm);
   }

   spin_lock(&inode->vfs_inode.i_lock);
   FhgfsInode_setLastWriteBackOrIsizeWriteTime(inode);
   FhgfsInode_decWriteBackCounter(inode);
   FhgfsInode_unsetNoIsizeDecrease(inode);
   spin_unlock(&inode->vfs_inode.i_lock);

   return comm->nodeResult;
}

int64_t FhgfsOpsCommKitVec_rwFileCommunicate(App* app, RemotingIOInfo* ioInfo,
   FhgfsCommKitVec* comm, Fhgfs_RWType rwType)
{
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);
   int64_t retVal;
   unsigned retries =  0;
   unsigned maxRetries = Config_getConnNumCommRetries(cfg);
   bool isSignalPending = false;
   const char* logCommInterruptedTxt = "Communication interrupted by signal";

   CommKitVecHelper commHelper =
   {
      .app = app,
      .log = App_getLogger(app),
      .logContext = (rwType == BEEGFS_RWTYPE_READ) ?
         "readfile (vec communication)" : "writefile (vec communication)",
      .ioInfo = ioInfo,
   };

   do
   {
      FhgfsChunkPageVec_resetIterator(comm->pageVec);

      if (rwType == BEEGFS_RWTYPE_READ)
         retVal = __FhgfsOpsCommKitVec_readfileCommunicate(&commHelper, comm);
      else
         retVal = __FhgfsOpsCommKitVec_writefileCommunicate(&commHelper, comm);

      if (unlikely(retVal == -FhgfsOpsErr_AGAIN || retVal == -FhgfsOpsErr_COMMUNICATION) )
      {
         if (fatal_signal_pending(current))
         { // interrupted by signal
            Logger_logTopFormatted(log, LogTopic_COMMKIT, Log_NOTICE, __func__,
               "%s.", logCommInterruptedTxt);

            isSignalPending = true;
         }
         else
            MessagingTk_waitBeforeRetry(retries);

         retries++;
      }

      if (unlikely(retVal == -FhgfsOpsErr_AGAIN ) )
      {
         retVal = -FhgfsOpsErr_COMMUNICATION;
         retries = 0;
      }

   } while (retVal == -FhgfsOpsErr_COMMUNICATION && retries <= maxRetries && !isSignalPending);

   if (rwType == BEEGFS_RWTYPE_READ)
      __FhgfsOpsCommKitVec_readfileStageHandlePages(&commHelper, comm);
   else
      __FhgfsOpsCommKitVec_writefileStageHandlePages(&commHelper, comm);

   return retVal;
}
