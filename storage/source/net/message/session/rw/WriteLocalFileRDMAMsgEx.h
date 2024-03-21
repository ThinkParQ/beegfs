#ifndef WRITELOCALFILERDMAMSGEX_H_
#define WRITELOCALFILERDMAMSGEX_H_

#ifdef BEEGFS_NVFS
#include <common/net/message/session/rw/WriteLocalFileRDMAMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRDMARespMsg.h>
#include <common/components/worker/Worker.h>
#include <session/SessionLocalFile.h>
#include <common/storage/StorageErrors.h>
#include "WriteLocalFileMsgEx.h"


/**
 * Implements RDMA read protocol.
 */
class WriteLocalFileRDMAMsgSender : public WriteLocalFileRDMAMsg
{
   public:
      struct WriteState : public WriteStateBase
      {
         RdmaInfo* rdma;
         uint64_t rBuf;
         size_t rLen;
         uint64_t rOff;
         int64_t recvSize;

         WriteState(const char* logContext, ssize_t exactStaticRecvSize,
            int64_t toBeReceived, off_t writeOffset, SessionLocalFile* sessionLocalFile) :
            WriteStateBase(logContext, exactStaticRecvSize, toBeReceived, writeOffset,
               sessionLocalFile)
         {
            recvSize = toBeReceived;
         }
      };

   private:
      friend class WriteLocalFileMsgExBase<WriteLocalFileRDMAMsgSender, WriteState>;

      static const std::string logContextPref;

      ssize_t recvPadding(ResponseContext& ctx, int64_t toBeReceived);

      inline void sendResponse(ResponseContext& ctx, int err)
      {
         ctx.sendResponse(WriteLocalFileRDMARespMsg(err));
      }

      inline bool writeStateInit(WriteState& ws)
      {
         ws.rdma = getRdmaInfo();
         if (unlikely(!ws.rdma->next(ws.rBuf, ws.rLen, ws.rOff)))
         {
            LogContext(ws.logContext).logErr("No entities in RDMA buffers.");
            return false;
         }
         return true;
      }

      inline ssize_t writeStateRecvData(ResponseContext& ctx, WriteState& ws)
      {
         // Cannot RDMA anything larger than WORKER_BUFIN_SIZE in a single operation
         // because that is the size of the buffer passed in by the Worker.
         // TODO: pass around a Buffer with a length instead of unqualified char*.
         ws.recvLength = BEEGFS_MIN(
            BEEGFS_MIN(
               BEEGFS_MIN(ws.exactStaticRecvSize, ws.toBeReceived),
               (ssize_t)(ws.rLen - ws.rOff)),
            WORKER_BUFIN_SIZE);
         return ctx.getSocket()->read(ctx.getBuffer(), ws.recvLength, 0, ws.rBuf + ws.rOff, ws.rdma->key);
      }

      inline size_t writeStateNext(WriteState& ws, ssize_t writeRes)
      {
         ws.rOff += writeRes;
         if (ws.toBeReceived > 0 && ws.rOff == ws.rLen)
         {
            if (unlikely(!ws.rdma->next(ws.rBuf, ws.rLen, ws.rOff)))
            {
               LogContext(ws.logContext).logErr("RDMA buffers expended but not all data received. toBeReceived=" +
                  StringTk::uint64ToStr(ws.toBeReceived) + "; "
                  "target: " + StringTk::uintToStr(ws.sessionLocalFile->getTargetID() ) + "; "
                  "file: " + ws.sessionLocalFile->getFileID() + "; ");
               return ws.recvSize - ws.toBeReceived;
            }
         }
         return 0;
      }

};

typedef WriteLocalFileMsgExBase<WriteLocalFileRDMAMsgSender,
                                WriteLocalFileRDMAMsgSender::WriteState> WriteLocalFileRDMAMsgEx;

#endif /* BEEGFS_NVFS */

#endif // WRITELOCALFILERDMAMSGEX_H_
