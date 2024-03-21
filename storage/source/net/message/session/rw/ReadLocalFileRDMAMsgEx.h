#ifndef READLOCALFILERDMAMSGEX_H_
#define READLOCALFILERDMAMSGEX_H_

#ifdef BEEGFS_NVFS
#include <string>
#include <typeinfo>
#include <common/net/message/session/rw/ReadLocalFileRDMAMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/components/worker/Worker.h>
#include <session/SessionLocalFileStore.h>
#include "ReadLocalFileV2MsgEx.h"

/**
 * Implements RDMA write protocol.
 */
class ReadLocalFileRDMAMsgSender : public ReadLocalFileRDMAMsg
{
   public:
      struct ReadState : public ReadStateBase
      {
         RdmaInfo* rdma;
         uint64_t rBuf;
         size_t rLen;
         uint64_t rOff;

         ReadState(const char* logContext, uint64_t toBeRead,
            SessionLocalFile* sessionLocalFile) :
            ReadStateBase(logContext, toBeRead, sessionLocalFile) {}

      };

   private:
      friend class ReadLocalFileMsgExBase<ReadLocalFileRDMAMsgSender, ReadState>;

      static std::string logContextPref;

      inline void sendLengthInfo(Socket* sock, int64_t lengthInfo)
      {
         lengthInfo = HOST_TO_LE_64(lengthInfo);
         sock->send(&lengthInfo, sizeof(int64_t), 0);
      }

      /**
       * RDMA write data to the remote buffer.
       */
      inline ssize_t readStateSendData(Socket* sock, ReadState& rs, char* buf, bool isFinal)
      {
         ssize_t writeRes = sock->write(buf, rs.readRes, 0, rs.rBuf + rs.rOff, rs.rdma->key);
         LOG_DEBUG(rs.logContext, Log_DEBUG,
            "buf: " + StringTk::uint64ToHexStr((uint64_t)buf) + "; "
            "bufLen: " + StringTk::int64ToStr(rs.readRes) + "; "
            "rbuf: " + StringTk::uint64ToHexStr(rs.rBuf) + "; "
            "rkey: " + StringTk::uintToHexStr(rs.rdma->key) + "; "
            "writeRes: " + StringTk::int64ToStr(writeRes));

         if (unlikely(writeRes != rs.readRes))
         {
            LogContext(rs.logContext).logErr("Unable to write file data to client. "
               "FileID: " + rs.sessionLocalFile->getFileID() + "; "
               "SysErr: " + System::getErrString());
            writeRes = -1;
         }

         if (isFinal && likely(writeRes >= 0))
            sendLengthInfo(sock, getCount() - rs.toBeRead);

         return writeRes;
      }

      inline ssize_t getReadLength(ReadState& rs, ssize_t len)
      {
         // Cannot RDMA anything larger than WORKER_BUFOUT_SIZE in a single operation
         // because that is the size of the buffer passed in by the Worker.
         // TODO: pass around a Buffer with a length instead of unqualified char*.
         return BEEGFS_MIN(BEEGFS_MIN(len, ssize_t(rs.rLen - rs.rOff)), WORKER_BUFOUT_SIZE);
      }

      inline bool readStateInit(ReadState& rs)
      {
         rs.rdma = getRdmaInfo();
         if (unlikely(!rs.rdma->next(rs.rBuf, rs.rLen, rs.rOff)))
         {
            LogContext(rs.logContext).logErr("No entities in RDMA buffers.");
            return false;
         }
         return true;
      }

      inline bool readStateNext(ReadState& rs)
      {
         rs.rOff += rs.readRes;
         if (rs.rOff == rs.rLen)
         {
            if (unlikely(!rs.rdma->next(rs.rBuf, rs.rLen, rs.rOff)))
            {
               LogContext(rs.logContext).logErr("RDMA buffers exhausted");
               return false;
            }
         }
         return true;
      }

      inline size_t getBuffers(ResponseContext& ctx, char** dataBuf, char** sendBuf)
      {
         *dataBuf = ctx.getBuffer();
         *sendBuf = *dataBuf;
         return ctx.getBufferLength();
      }
};

typedef ReadLocalFileMsgExBase<ReadLocalFileRDMAMsgSender,
                               ReadLocalFileRDMAMsgSender::ReadState> ReadLocalFileRDMAMsgEx;

#endif /* BEEGFS_NVFS */

#endif /*READLOCALFILERDMAMSGEX_H_*/
