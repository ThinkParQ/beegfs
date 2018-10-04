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

   int openFlags = O_WRONLY | O_CREAT;
   SessionQuotaInfo quotaInfo(false, false, 0, 0);
  // mode_t fileMode = STORAGETK_DEFAULTCHUNKFILEMODE;

   int targetFD;
   int fd;
   FhgfsOpsErr openRes;

   auto* const target = app->getStorageTargets()->getTarget(targetID);
   if (!target)
   {
      LogContext(__func__).logErr(
         "Error resyncing chunk; Could not open FD; chunkPath: "
            + relativeChunkPathStr);
      retVal = FhgfsOpsErr_PATHNOTEXISTS;
      goto send_response;
   }

   targetFD = *target->getMirrorFD();

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

