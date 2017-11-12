#ifndef TRUNCFILEMSGEX_H_
#define TRUNCFILEMSGEX_H_

#include <common/net/message/storage/TruncFileMsg.h>
#include <storage/MetaStore.h>


class TruncFileMsgEx : public TruncFileMsg
{
   public:
      TruncFileMsgEx() : TruncFileMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr truncFileRec(Path& path);
      FhgfsOpsErr truncLocalFile(FileInode* file, Path& path);
      int64_t getNodeLocalOffset(int64_t pos, int64_t chunkSize, size_t numNodes,
         size_t stripeNodeIndex);
      int64_t getNodeLocalTruncPos(int64_t pos, StripePattern& pattern,
         size_t stripeNodeIndex);
};


#endif /*TRUNCFILEMSGEX_H_*/
