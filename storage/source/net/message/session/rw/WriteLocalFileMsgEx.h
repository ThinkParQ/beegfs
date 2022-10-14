#ifndef WRITELOCALFILEMSGEX_H_
#define WRITELOCALFILEMSGEX_H_

#include <common/net/message/session/rw/WriteLocalFileMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#include <session/SessionLocalFile.h>
#include <common/storage/StorageErrors.h>


#define WRITEMSG_MIRROR_RETRIES_NUM    1

class StorageTarget;

/**
 * Contains common data needed by implementations of the network protocol
 * that receive data from the client.
 */
struct WriteStateBase
{
   const char* logContext;
   ssize_t exactStaticRecvSize;
   ssize_t recvLength;
   int64_t toBeReceived;
   off_t writeOffset;
   SessionLocalFile* sessionLocalFile;

   WriteStateBase(const char* logContext, ssize_t exactStaticRecvSize,
      int64_t toBeReceived, off_t writeOffset, SessionLocalFile* sessionLocalFile)
   {
      this->logContext = logContext;
      this->exactStaticRecvSize = exactStaticRecvSize;
      this->toBeReceived = toBeReceived;
      this->writeOffset = writeOffset;
      this->sessionLocalFile = sessionLocalFile;
      recvLength = BEEGFS_MIN(exactStaticRecvSize, toBeReceived);
   }

};


template <class Msg, typename WriteState>
class WriteLocalFileMsgExBase : public Msg
{

   private:
      Socket* mirrorToSock;
      unsigned mirrorRetriesLeft;

   public:
      bool processIncoming(NetMessage::ResponseContext& ctx);

      WriteLocalFileMsgExBase() : Msg()
      {
         mirrorToSock = NULL;
         mirrorRetriesLeft = WRITEMSG_MIRROR_RETRIES_NUM;
      }

   private:
      std::pair<bool, int64_t> write(NetMessage::ResponseContext& ctx);

      ssize_t doWrite(int fd, char* buf, size_t count, off_t offset, int& outErrno);

      FhgfsOpsErr openFile(const StorageTarget& target, SessionLocalFile* sessionLocalFile);

      FhgfsOpsErr prepareMirroring(char* buf, size_t bufLen,
         SessionLocalFile* sessionLocalFile, StorageTarget& target);
      FhgfsOpsErr sendToMirror(const char* buf, size_t bufLen, int64_t offset, int64_t toBeMirrored,
         SessionLocalFile* sessionLocalFile);
      FhgfsOpsErr finishMirroring(SessionLocalFile* sessionLocalFile, StorageTarget& target);

      bool doSessionCheck();

      int64_t incrementalRecvAndWriteStateful(NetMessage::ResponseContext& ctx,
         SessionLocalFile* sessionLocalFile);

      void incrementalRecvPadding(NetMessage::ResponseContext& ctx, int64_t padLen,
         SessionLocalFile* sessionLocalFile);

      inline ssize_t recvPadding(NetMessage::ResponseContext& ctx, int64_t toBeReceived)
      {
         return static_cast<Msg&>(*this).recvPadding(ctx, toBeReceived);
      }

      inline void sendResponse(NetMessage::ResponseContext& ctx, int err)
      {
         return static_cast<Msg&>(*this).sendResponse(ctx, err);
      }

      inline bool writeStateInit(WriteState& ws)
      {
         return static_cast<Msg&>(*this).writeStateInit(ws);
      }

      inline ssize_t writeStateRecvData(NetMessage::ResponseContext& ctx, WriteState& ws)
      {
         return static_cast<Msg&>(*this).writeStateRecvData(ctx, ws);
      }

      inline size_t writeStateNext(WriteState& ws, ssize_t writeRes)
      {
         return static_cast<Msg&>(*this).writeStateNext(ws, writeRes);
      }

   public:
      inline bool isMsgValid() const
      {
         return static_cast<const Msg&>(*this).isMsgValid();
      }

      inline bool isMsgHeaderFeatureFlagSet(unsigned flag) const
      {
         return static_cast<const Msg&>(*this).isMsgHeaderFeatureFlagSet(flag);
      }

      inline unsigned getMsgHeaderUserID() const
      {
         return static_cast<const Msg&>(*this).getMsgHeaderUserID();
      }

      inline uint16_t getTargetID() const
      {
         return static_cast<const Msg&>(*this).getTargetID();
      }

      inline int64_t getOffset() const
      {
         return static_cast<const Msg&>(*this).getOffset();
      }

      inline unsigned getUserID() const
      {
         return static_cast<const Msg&>(*this).getUserID();
      }

      inline unsigned getGroupID() const
      {
         return static_cast<const Msg&>(*this).getGroupID();
      }

      inline int64_t getCount() const
      {
         return static_cast<const Msg&>(*this).getCount();
      }

      inline const char* getFileHandleID()
      {
         return static_cast<Msg&>(*this).getFileHandleID();
      }

      inline NumNodeID getClientNumID() const
      {
         return static_cast<const Msg&>(*this).getClientNumID();
      }

      inline unsigned getAccessFlags() const
      {
         return static_cast<const Msg&>(*this).getAccessFlags();
      }

      inline PathInfo* getPathInfo ()
      {
         return static_cast<Msg&>(*this).getPathInfo();
      }
};

/**
 * Implements the recv protocol.
 */
class WriteLocalFileMsgSender : public WriteLocalFileMsg
{
   public:
      struct WriteState : public WriteStateBase
      {
         WriteState(const char* logContext, ssize_t exactStaticRecvSize,
            int64_t toBeReceived, off_t writeOffset, SessionLocalFile* sessionLocalFile) :
            WriteStateBase(logContext, exactStaticRecvSize, toBeReceived, writeOffset,
               sessionLocalFile) {}
      };

   private:
      friend class WriteLocalFileMsgExBase<WriteLocalFileMsgSender, WriteState>;

      static const std::string logContextPref;

      ssize_t recvPadding(ResponseContext& ctx, int64_t toBeReceived);

      inline void sendResponse(ResponseContext& ctx, int err)
      {
         ctx.sendResponse(WriteLocalFileRespMsg(err));
      }

      inline bool writeStateInit(WriteState& ws)
      {
         return true;
      }

      inline ssize_t writeStateRecvData(ResponseContext& ctx, WriteState& ws)
      {
         AbstractApp* app = PThread::getCurrentThreadApp();
         int connMsgMediumTimeout = app->getCommonConfig()->getConnMsgMediumTimeout();
         ws.recvLength = BEEGFS_MIN(ws.exactStaticRecvSize, ws.toBeReceived);
         return ctx.getSocket()->recvExactT(ctx.getBuffer(), ws.recvLength, 0, connMsgMediumTimeout);
      }

      inline size_t writeStateNext(WriteState& ws, ssize_t writeRes)
      {
         return 0;
      }

};

typedef WriteLocalFileMsgExBase<WriteLocalFileMsgSender,
                                WriteLocalFileMsgSender::WriteState> WriteLocalFileMsgEx;

#endif /*WRITELOCALFILEMSGEX_H_*/
