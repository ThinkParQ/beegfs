#ifndef MOVINGFILEINSERTMSGEX_H_
#define MOVINGFILEINSERTMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/moving/MovingFileInsertMsg.h>
#include <storage/MetaStore.h>

// this class is used on the server where the file is moved to

class MovingFileInsertMsgEx : public MovingFileInsertMsg
{
   public:
      MovingFileInsertMsgEx() : MovingFileInsertMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr insert(FileInode** outUnlinkedFile);
};

#endif /*MOVINGFILEINSERTMSGEX_H_*/
