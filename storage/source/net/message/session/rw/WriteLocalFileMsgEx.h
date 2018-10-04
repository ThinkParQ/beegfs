#ifndef WRITELOCALFILEMSGEX_H_
#define WRITELOCALFILEMSGEX_H_

#include <common/net/message/session/rw/WriteLocalFileMsg.h>
#include <session/SessionLocalFile.h>
#include <common/storage/StorageErrors.h>


#define WRITEMSG_MIRROR_RETRIES_NUM    1

class StorageTarget;

class WriteLocalFileMsgEx : public WriteLocalFileMsg
{
   public:
      WriteLocalFileMsgEx() : WriteLocalFileMsg()
      {
         mirrorToSock = NULL;
         mirrorRetriesLeft = WRITEMSG_MIRROR_RETRIES_NUM;
      }

      virtual bool processIncoming(ResponseContext& ctx);

   private:
      Socket* mirrorToSock;
      unsigned mirrorRetriesLeft;

      std::pair<bool, int64_t> write(ResponseContext& ctx);

      int64_t incrementalRecvAndWriteStateful(ResponseContext& ctx,
         SessionLocalFile* sessionLocalFile);
      void incrementalRecvPadding(ResponseContext& ctx, int64_t padLen,
         SessionLocalFile* sessionLocalFile);

      ssize_t doWrite(int fd, char* buf, size_t count, off_t offset, int& outErrno);

      FhgfsOpsErr openFile(const StorageTarget& target, SessionLocalFile* sessionLocalFile);

      FhgfsOpsErr prepareMirroring(char* buf, size_t bufLen,
         SessionLocalFile* sessionLocalFile, StorageTarget& target);
      FhgfsOpsErr sendToMirror(const char* buf, size_t bufLen, int64_t offset, int64_t toBeMirrored,
         SessionLocalFile* sessionLocalFile);
      FhgfsOpsErr finishMirroring(SessionLocalFile* sessionLocalFile, StorageTarget& target);

      bool doSessionCheck();
};

#endif /*WRITELOCALFILEMSGEX_H_*/
