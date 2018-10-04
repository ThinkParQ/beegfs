#ifndef READLOCALFILEV2MSGEX_H_
#define READLOCALFILEV2MSGEX_H_

#include <common/net/message/session/rw/ReadLocalFileV2Msg.h>
#include <common/storage/StorageErrors.h>
#include <session/SessionLocalFileStore.h>

class StorageTarget;

class ReadLocalFileV2MsgEx : public ReadLocalFileV2Msg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      SessionLocalFileStore* sessionLocalFiles;


      int64_t incrementalReadStatefulAndSendV2(ResponseContext& ctx,
         SessionLocalFile* sessionLocalFile);

      void checkAndStartReadAhead(SessionLocalFile* sessionLocalFile, ssize_t readAheadTriggerSize,
         off_t currentOffset, off_t readAheadSize);

      FhgfsOpsErr openFile(const StorageTarget& target, SessionLocalFile* sessionLocalFile);


      // inliners
      
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
       * Send length information and the corresponding data packet buffer.
       *
       * Note: lengthInfo is used to compute buf length for send()
       * 
       * @param lengthInfo must not be negative
       * @param buf the buffer with a preceding gap for the length info
       * @param isFinal true if this is the last send, i.e. we have read all data
       */
      inline void sendLengthInfoAndData(Socket* sock, int64_t lengthInfo, char* buf, bool isFinal)
      {
         {
            Serializer ser(buf, sizeof(int64_t));
            ser % lengthInfo;
         }

         if (isFinal)
         {
            Serializer ser(buf + sizeof(int64_t) + lengthInfo, sizeof(int64_t));
            ser % int64_t(0);

            sock->send(buf, (2*sizeof(int64_t) ) + lengthInfo, 0);
         }
         else
         {
            sock->send(buf, sizeof(int64_t) + lengthInfo, 0);
         }
      }
};


#endif /*READLOCALFILEV2MSGEX_H_*/
