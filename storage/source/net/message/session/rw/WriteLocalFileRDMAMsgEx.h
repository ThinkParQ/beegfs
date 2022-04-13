#ifndef WRITELOCALFILERDMAMSGEX_H_
#define WRITELOCALFILERDMAMSGEX_H_

#ifdef BEEGFS_NVFS
#include <common/net/message/session/rw/WriteLocalFileRDMAMsg.h>
#include "WriteLocalFileMsgEx.h"
#include <session/SessionLocalFile.h>
#include <common/storage/StorageErrors.h>

// TBD: This class unnecessarily duplicates the logic of WriteLocalFileMsgEx
// with a few modifications to support RDMA. This was done to satisfy the design
// requirement of minimizing changes to the upstream code. A future project
// should refactor this and WriteLocalFileMsgEx with a delegation pattern to
// reduce duplicated code.

class WriteLocalFileRDMAMsgEx : public WriteLocalFileRDMAMsg
{
   public:
      WriteLocalFileRDMAMsgEx() : WriteLocalFileRDMAMsg()
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

#endif /* BEEGFS_NVFS */

#endif // WRITELOCALFILERDMAMSGEX_H_
