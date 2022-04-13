#ifndef READLOCALFILERDMAMSGEX_H_
#define READLOCALFILERDMAMSGEX_H_

#ifdef BEEGFS_NVFS
#include <string>
#include <typeinfo>
#include <common/net/message/session/rw/ReadLocalFileRDMAMsg.h>
#include <common/storage/StorageErrors.h>
#include <session/SessionLocalFileStore.h>

class StorageTarget;

class ReadLocalFileRDMAMsgEx : public ReadLocalFileRDMAMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      SessionLocalFileStore* sessionLocalFiles;


      int64_t incrementalReadStatefulAndSendRDMA(ResponseContext& ctx,
         SessionLocalFile* sessionLocalFile);

      void checkAndStartReadAhead(SessionLocalFile* sessionLocalFile, ssize_t readAheadTriggerSize,
         off_t currentOffset, off_t readAheadSize);

      FhgfsOpsErr openFile(const StorageTarget& target, SessionLocalFile* sessionLocalFile);


      // inliners

      /**
       * Send error code upon failure.
       */
      inline void sendError(ResponseContext& ctx, int64_t errorCode)
      {
         char buf[sizeof(uint64_t)];
         Serializer ser(buf, sizeof(uint64_t));

         LOG_DEBUG("ReadLocalFileRDMAMsgEx:sendError", Log_DEBUG, "error: " + StringTk::uint64ToHexStr((uint64_t)errorCode));
         errorCode = HOST_TO_LE_64(errorCode);
         ser % errorCode;
         ctx.getSocket()->send(buf, sizeof(uint64_t), 0);
      }

      /**
       * Write data to the remote buffer.
       */
      inline ssize_t writeData(ResponseContext &ctx, char *buf, size_t bufLen, uint64_t rBuf, unsigned rKey)
      {
         ssize_t writeRes = 0;
         LOG_DEBUG("ReadLocalFileRDMAMsgEx:writeData", Log_DEBUG,
            "buf: " + StringTk::uint64ToHexStr((uint64_t)buf) + "   "
            "bufLen: " + StringTk::int64ToStr(bufLen) + "   "
            "rbuf: " + StringTk::uint64ToHexStr(rBuf) + "   "
            "rkey: " + StringTk::uintToHexStr(rKey));
         if (bufLen > 0)
         {
            writeRes = ctx.getSocket()->write(buf, bufLen, 0, rBuf, rKey);
         }
         return writeRes;
      }

      /**
       * Send length of data written.
       */
      inline void sendDone(ResponseContext &ctx, int64_t lengthInfo)
      {
         char buf[sizeof(uint64_t)];
         Serializer ser(buf, sizeof(uint64_t));

         LOG_DEBUG("ReadLocalFileRDMAMsgEx:sendDone", Log_DEBUG, "length: " + StringTk::uint64ToHexStr((uint64_t)lengthInfo));
         ser % lengthInfo;
         ctx.getSocket()->send(buf, sizeof(uint64_t), 0);
      }
};
#endif /* BEEGFS_NVFS */

#endif /*READLOCALFILERDMAMSGEX_H_*/
