#ifndef FLOCKENTRYMSGEX_H_
#define FLOCKENTRYMSGEX_H_

#include <common/net/message/session/locking/FLockEntryMsg.h>
#include <common/storage/StorageErrors.h>
#include <storage/FileInode.h>


class FLockEntryMsgEx : public FLockEntryMsg
{
   public:
      FLockEntryMsgEx() : FLockEntryMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:

   private:

};

#endif /* FLOCKENTRYMSGEX_H_ */
