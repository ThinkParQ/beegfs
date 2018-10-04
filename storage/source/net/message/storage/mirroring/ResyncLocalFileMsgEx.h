#ifndef RESYNCLOCALFILEMSGEX_H_
#define RESYNCLOCALFILEMSGEX_H_

#include <common/net/message/storage/mirroring/ResyncLocalFileMsg.h>
#include <common/storage/StorageErrors.h>

class ResyncLocalFileMsgEx : public ResyncLocalFileMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      bool doWrite(int fd, const char* buf, size_t count, off_t offset, int& outErrno);
      bool doWriteSparse(int fd, const char* buf, size_t count, off_t offset, int& outErrno);
      bool doTrunc(int fd, off_t length, int& outErrno);
};

#endif /*RESYNCLOCALFILEMSGEX_H_*/
