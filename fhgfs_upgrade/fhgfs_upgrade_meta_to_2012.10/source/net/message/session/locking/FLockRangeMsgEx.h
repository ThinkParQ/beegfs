#ifndef FLOCKRANGEMSGEX_H_
#define FLOCKRANGEMSGEX_H_

#include <common/net/message/session/locking/FLockRangeMsg.h>
#include <common/storage/StorageErrors.h>
#include <storage/FileInode.h>


class FLockRangeMsgEx : public FLockRangeMsg
{
   public:
      FLockRangeMsgEx() : FLockRangeMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:

   private:

};

#endif /* FLOCKRANGEMSGEX_H_ */
