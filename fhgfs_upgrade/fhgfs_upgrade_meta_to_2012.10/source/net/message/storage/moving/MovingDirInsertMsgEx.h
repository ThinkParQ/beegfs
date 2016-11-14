#ifndef MOVINGDIRINSERTMSGEX_H_
#define MOVINGDIRINSERTMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/moving/MovingDirInsertMsg.h>
#include <storage/MetaStore.h>

// Move directory to another meta-data server

class MovingDirInsertMsgEx : public MovingDirInsertMsg
{
   public:
      MovingDirInsertMsgEx() : MovingDirInsertMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr insert(void);
};

#endif /*MOVINGDIRINSERTMSGEX_H_*/
