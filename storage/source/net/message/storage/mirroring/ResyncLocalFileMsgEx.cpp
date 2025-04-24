#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/mirroring/ResyncLocalFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <toolkit/StorageTkEx.h>

#include <program/Program.h>

#include "ResyncLocalFileMsgEx.h"

bool ResyncLocalFileMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   ChunkStore* chunkStore = app->getChunkDirStore();
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   const char* dataBuf = getDataBuf();
   uint16_t targetID = getResyncToTargetID();
   size_t count = getCount();
   int64_t offset = getOffset();
   std::string relativeChunkPathStr = getRelativePathStr();
   int writeErrno;
   bool writeRes;
   StorageTarget* target;

   int openFlags = O_WRONLY | O_CREAT;
   SessionQuotaInfo quotaInfo(false, false, 0, 0);
  // mode_t fileMode = STORAGETK_DEFAULTCHUNKFILEMODE;

   int targetFD;
   int fd;
   FhgfsOpsErr openRes;

   // should only be used for chunk balancing, to sync data to both buddies
   if(isMsgHeaderFeatureFlagSet(RESYNCLOCALFILEMSG_FLAG_CHUNKBALANCE_BUDDYMIRROR) )
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();
      targetID = isMsgHeaderFeatureFlagSet(RESYNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND) ?
         mirrorBuddies->getSecondaryTargetID(targetID) :
         mirrorBuddies->getPrimaryTargetID(targetID);

      if(unlikely(!targetID) )
      { // unknown target
         LogContext(__func__).logErr("Invalid mirror buddy group ID: " +
            StringTk::uintToStr(!targetID ) );
         retVal = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }
   }

   target = app->getStorageTargets()->getTarget(targetID);
   if (!target)
   {
      LogContext(__func__).logErr(
         "Error resyncing chunk; Could not open FD; chunkPath: "
            + relativeChunkPathStr);
      retVal = FhgfsOpsErr_PATHNOTEXISTS;
      goto send_response;
   }

   retVal = forwardToSecondary(*target, ctx);
   //check if path is relative to buddy mirror dir or chunks dir
   targetFD = isMsgHeaderFeatureFlagSet(RESYNCLOCALFILEMSG_FLAG_BUDDYMIRROR) 
         ? *target->getMirrorFD()
         : *target->getChunkFD();
    
   // always truncate when we write the very first block of a file
   if(!offset && !isMsgHeaderFeatureFlagSet (RESYNCLOCALFILEMSG_FLAG_NODATA) )
      openFlags |= O_TRUNC;

   openRes = chunkStore->openChunkFile(targetFD, NULL, relativeChunkPathStr, true,
      openFlags, &fd, &quotaInfo, {});

   if (openRes != FhgfsOpsErr_SUCCESS)
   {
      LogContext(__func__).logErr(
         "Error resyncing chunk; Could not open FD; chunkPath: "
            + relativeChunkPathStr);
      retVal = FhgfsOpsErr_PATHNOTEXISTS;

      target->setState(TargetConsistencyState_BAD);

      goto send_response;
   }

   if(isMsgHeaderFeatureFlagSet (RESYNCLOCALFILEMSG_FLAG_NODATA)) // do not sync actual data
      goto set_attribs;

   if(isMsgHeaderFeatureFlagSet (RESYNCLOCALFILEMSG_CHECK_SPARSE))
      writeRes = doWriteSparse(fd, dataBuf, count, offset, writeErrno);
   else
      writeRes = doWrite(fd, dataBuf, count, offset, writeErrno);

   if(unlikely(!writeRes) )
   { // write error occured (could also be e.g. disk full)
      LogContext(__func__).logErr(
         "Error resyncing chunk; chunkPath: " + relativeChunkPathStr + "; error: "
            + System::getErrString(writeErrno));

      target->setState(TargetConsistencyState_BAD);

      retVal = FhgfsOpsErrTk::fromSysErr(writeErrno);
   }

   if(isMsgHeaderFeatureFlagSet (RESYNCLOCALFILEMSG_FLAG_TRUNC))
   {
      int truncErrno;
      // we trunc after a possible write, so we need to trunc at offset+count
      bool truncRes = doTrunc(fd, offset + count, truncErrno);

      if(!truncRes)
      {
         LogContext(__func__).logErr(
            "Error resyncing chunk; chunkPath: " + relativeChunkPathStr + "; error: "
               + System::getErrString(truncErrno));

         target->setState(TargetConsistencyState_BAD);

         retVal = FhgfsOpsErrTk::fromSysErr(truncErrno);
      }
   }

set_attribs:

   if(isMsgHeaderFeatureFlagSet(RESYNCLOCALFILEMSG_FLAG_SETATTRIBS) &&
      (retVal == FhgfsOpsErr_SUCCESS))
   {
      SettableFileAttribs* attribs = getChunkAttribs();
      // update mode
      int chmodRes = fchmod(fd, attribs->mode);
      if(chmodRes == -1)
      { // could be an error
         int errCode = errno;

         if(errCode != ENOENT)
         { // unhandled chmod() error
            LogContext(__func__).logErr("Unable to change file mode: " + relativeChunkPathStr
               + ". SysErr: " + System::getErrString());
         }
      }

      // update UID and GID...
      int chownRes = fchown(fd, attribs->userID, attribs->groupID);
      if(chownRes == -1)
      { // could be an error
         int errCode = errno;

         if(errCode != ENOENT)
         { // unhandled chown() error
            LogContext(__func__).logErr( "Unable to change file owner: " + relativeChunkPathStr
               + ". SysErr: " + System::getErrString());
         }
      }

      if((chmodRes == -1) || (chownRes == -1))
      {
         target->setState(TargetConsistencyState_BAD);
         retVal = FhgfsOpsErr_INTERNAL;
      }
   }

   close(fd);


send_response:
   ctx.sendResponse(ResyncLocalFileRespMsg(retVal) );

   return true;
}

/**
 * Write until everything was written (handle short-writes) or an error occured
 */
bool ResyncLocalFileMsgEx::doWrite(int fd, const char* buf, size_t count, off_t offset,
   int& outErrno)
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
         return false;
      }

      sumWriteRes += writeRes;

   } while (sumWriteRes != count);

   return true;
}

/**
 * Write until everything was written (handle short-writes) or an error occured
 */
bool ResyncLocalFileMsgEx::doWriteSparse(int fd, const char* buf, size_t count, off_t offset,
   int& outErrno)
{
   size_t sumWriteRes = 0;
   const char zeroBuf[ RESYNCER_SPARSE_BLOCK_SIZE ] = { 0 };

   do
   {
      size_t cmpLen = BEEGFS_MIN(count - sumWriteRes, RESYNCER_SPARSE_BLOCK_SIZE);
      int cmpRes = memcmp(buf + sumWriteRes, zeroBuf, cmpLen);

      if(!cmpRes)
      { // sparse area
         sumWriteRes += cmpLen;

         if(sumWriteRes == count)
         { // end of buf
            // we must trunc here because this might be the end of the file
            int truncRes = ftruncate(fd, offset+count);

            if(unlikely(truncRes == -1) )
            {
               outErrno = errno;
               return false;
            }
         }
      }
      else
      { // non-sparse area
         ssize_t writeRes = MsgHelperIO::pwrite(fd, buf + sumWriteRes, cmpLen,
            offset + sumWriteRes);

         if(unlikely(writeRes == -1))
         {
            outErrno = errno;
            return false;
         }

         sumWriteRes += writeRes;
      }

   } while (sumWriteRes != count);

   return true;
}

bool ResyncLocalFileMsgEx::doTrunc(int fd, off_t length, int& outErrno)
{
   int truncRes = ftruncate(fd, length);

   if (truncRes == -1)
   {
      outErrno = errno;
      return false;
   }

   return true;
}

/**
 * If this is a buddy mirror msg and we are the primary, forward this msg to secondary.
 *
 * @return _COMMUNICATION if forwarding to buddy failed and buddy is not marked offline (in which
 *    case *outChunkLocked==false is guaranteed).
 * @throw SocketException if sending of GenericResponseMsg fails.
 */
FhgfsOpsErr ResyncLocalFileMsgEx::forwardToSecondary(StorageTarget& target, ResponseContext& ctx)
{
   const char* logContext = "ResyncLocalFileMsg incoming (forward to secondary)";

   App* app = Program::getApp();

   if(!isMsgHeaderFeatureFlagSet(RESYNCLOCALFILEMSG_FLAG_CHUNKBALANCE_BUDDYMIRROR))
      return FhgfsOpsErr_SUCCESS; // nothing to do

   // instead of creating a new msg object, we just re-use "this" with "buddymirror second" flag
   addMsgHeaderFeatureFlag(RESYNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);

   RequestResponseArgs rrArgs(NULL, this, NETMSGTYPE_ResyncLocalFileResp);
   RequestResponseTarget rrTarget(getResyncToTargetID(), app->getTargetMapper(), app->getStorageNodes(),
      app->getTargetStateStore(), app->getMirrorBuddyGroupMapper(), true);

   FhgfsOpsErr commRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

   // remove the flag that we just added for secondary
   unsetMsgHeaderFeatureFlag(RESYNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);

   if(unlikely(
      (commRes == FhgfsOpsErr_COMMUNICATION) &&
      (rrTarget.outTargetReachabilityState == TargetReachabilityState_OFFLINE) ) )
   {
      LogContext(logContext).log(Log_DEBUG, "Secondary is offline and will need resync. " 
         "mirror buddy group ID: " + StringTk::uintToStr(getResyncToTargetID() ));

      // buddy is marked offline, so local msg processing will be done and buddy needs resync
      target.setBuddyNeedsResync(true);

      return FhgfsOpsErr_SUCCESS; // go ahead with local msg processing
   }

   if(unlikely(commRes != FhgfsOpsErr_SUCCESS) )
   {
      LogContext(logContext).log(Log_DEBUG, "Forwarding failed. "
         "mirror buddy group ID: " + StringTk::uintToStr(getResyncToTargetID() ) + "; "
         "error: " + std::to_string(commRes));

      std::string genericRespStr = "Communication with secondary failed. "
         "mirror buddy group ID: " + StringTk::uintToStr(getResyncToTargetID() );

      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR,
               std::move(genericRespStr)));

      return FhgfsOpsErr_COMMUNICATION;
   }

   ResyncLocalFileRespMsg* respMsg = (ResyncLocalFileRespMsg*)rrArgs.outRespMsg.get();
   FhgfsOpsErr secondaryRes = respMsg->getResult();
   if(unlikely(secondaryRes != FhgfsOpsErr_SUCCESS) )
   {
      if(secondaryRes == FhgfsOpsErr_UNKNOWNTARGET)
      {
         /* local msg processing shall be done and buddy needs resync
            (this is normal when a storage is restarted without a broken secondary target, so we
            report success to a client in this case) */

         LogContext(logContext).log(Log_DEBUG,
            "Secondary reports unknown target error and will need resync. "
            "mirror buddy group ID: " + StringTk::uintToStr(getResyncToTargetID() ) );

         target.setBuddyNeedsResync(true);

         return FhgfsOpsErr_SUCCESS;
      }

      LogContext(logContext).log(Log_NOTICE, std::string("Secondary reported error: ") +
         std::to_string(secondaryRes) + "; "
         "mirror buddy group ID: " + StringTk::uintToStr(getResyncToTargetID()) );

      return secondaryRes;
   }


   return FhgfsOpsErr_SUCCESS;
}
