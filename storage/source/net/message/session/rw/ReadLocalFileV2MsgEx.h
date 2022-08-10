#ifndef READLOCALFILEV2MSGEX_H_
#define READLOCALFILEV2MSGEX_H_

#include <common/net/message/session/rw/ReadLocalFileV2Msg.h>
#include <common/storage/StorageErrors.h>
#include <session/SessionLocalFileStore.h>

class StorageTarget;

/**
 * Contains common data needed by implementations of the network protocol
 * that send data to the client.
 */
struct ReadStateBase
{
   const char* logContext;
   uint64_t toBeRead;
   SessionLocalFile* sessionLocalFile;
   ssize_t readRes;

   ReadStateBase(const char* logContext, uint64_t toBeRead,
      SessionLocalFile* sessionLocalFile)
   {
      this->logContext = logContext;
      this->toBeRead = toBeRead;
      this->sessionLocalFile = sessionLocalFile;
   }
};

template <class Msg, typename ReadState>
class ReadLocalFileMsgExBase : public Msg
{
   public:
      bool processIncoming(NetMessage::ResponseContext& ctx);

   private:
      SessionLocalFileStore* sessionLocalFiles;

      FhgfsOpsErr openFile(const StorageTarget& target, SessionLocalFile* sessionLocalFile);

      void checkAndStartReadAhead(SessionLocalFile* sessionLocalFile, ssize_t readAheadTriggerSize,
         off_t currentOffset, off_t readAheadSize);

      int64_t incrementalReadStatefulAndSendV2(NetMessage::ResponseContext& ctx,
         SessionLocalFile* sessionLocalFile);

      inline void sendLengthInfo(Socket* sock, int64_t lengthInfo)
      {
         static_cast<Msg&>(*this).sendLengthInfo(sock, lengthInfo);
      }

      inline bool readStateInit(ReadState& rs)
      {
         return static_cast<Msg&>(*this).readStateInit(rs);
      }

      inline ssize_t readStateSendData(Socket* sock, ReadState& rs, char* buf, bool isFinal)
      {
         return static_cast<Msg&>(*this).readStateSendData(sock, rs, buf, isFinal);
      }

      inline bool readStateNext(ReadState& rs)
      {
         return static_cast<Msg&>(*this).readStateNext(rs);
      }

      inline ssize_t getReadLength(ReadState& rs, ssize_t len)
      {
         return static_cast<Msg&>(*this).getReadLength(rs, len);
      }

      inline size_t getBuffers(NetMessage::ResponseContext& ctx, char** dataBuf, char** sendBuf)
      {
         return static_cast<Msg&>(*this).getBuffers(ctx, dataBuf, sendBuf);
      }

   public:
      inline unsigned getMsgHeaderUserID() const
      {
         return static_cast<const Msg&>(*this).getMsgHeaderUserID();
      }

      inline bool isMsgHeaderFeatureFlagSet(unsigned flag) const
      {
         return static_cast<const Msg&>(*this).isMsgHeaderFeatureFlagSet(flag);
      }

      inline uint16_t getTargetID() const
      {
         return static_cast<const Msg&>(*this).getTargetID();
      }

      inline int64_t getOffset() const
      {
         return static_cast<const Msg&>(*this).getOffset();
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

      inline bool isMsgValid() const
      {
         return static_cast<const Msg&>(*this).isMsgValid();
      }

};

/**
 * Implements the Version 2 send protocol. It uses a preceding length info for each chunk.
 */
class ReadLocalFileV2MsgSender : public ReadLocalFileV2Msg
{
   /* note on protocol: this works by sending an int64 before each data chunk, which contains the
      length of the next data chunk; or a zero if no more data can be read; or a negative fhgfs
      error code in case of an error */
   public:
      struct ReadState : public ReadStateBase
      {
         ReadState(const char* logContext, uint64_t toBeRead,
            SessionLocalFile* sessionLocalFile) :
            ReadStateBase(logContext, toBeRead, sessionLocalFile) {}
      };

   private:
      friend class ReadLocalFileMsgExBase<ReadLocalFileV2MsgSender, ReadState>;

      static std::string logContextPref;

      /**
       * Send only length information without a data packet. Typically used for the final length
       * info at the end of the requested data.
       */
      inline void sendLengthInfo(Socket* sock, int64_t lengthInfo)
      {
         lengthInfo = HOST_TO_LE_64(lengthInfo);
         sock->send(&lengthInfo, sizeof(int64_t), 0);
      }

      /**
       * No-op for this implementation.
       */
      inline bool readStateInit(ReadState& rs)
      {
         return true;
      }

      /**
       * Send length information and the corresponding data packet buffer.
       *
       * Note: rs.readRes is used to compute buf length for send()
       *
       * @param rs.readRes must not be negative
       * @param buf the buffer with a preceding gap for the length info
       * @param isFinal true if this is the last send, i.e. we have read all data
       */
      inline ssize_t readStateSendData(Socket* sock, ReadState& rs, char* buf, bool isFinal)
      {
         ssize_t sendRes;
         {
            Serializer ser(buf, sizeof(int64_t));
            ser % rs.readRes;
         }

         if (isFinal)
         {
            Serializer ser(buf + sizeof(int64_t) + rs.readRes, sizeof(int64_t));
            ser % int64_t(0);
            sendRes = sock->send(buf, (2*sizeof(int64_t) ) + rs.readRes, 0);
         }
         else
         {
            sendRes = sock->send(buf, sizeof(int64_t) + rs.readRes, 0);
         }
         return sendRes;
      }

      /**
       * No-op for this implementation.
       */
      inline bool readStateNext(ReadState& rs)
      {
         return true;
      }

      inline ssize_t getReadLength(ReadState& rs, ssize_t len)
      {
         return len;
      }

      size_t getBuffers(ResponseContext& ctx, char** dataBuf, char** sendBuf);
};

typedef ReadLocalFileMsgExBase<ReadLocalFileV2MsgSender,
                               ReadLocalFileV2MsgSender::ReadState> ReadLocalFileV2MsgEx;

#endif /*READLOCALFILEV2MSGEX_H_*/
