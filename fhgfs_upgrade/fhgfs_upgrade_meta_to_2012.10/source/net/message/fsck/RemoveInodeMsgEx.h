#ifndef REMOVEINODEMSGEX_H
#define REMOVEINODEMSGEX_H

#include <common/net/message/fsck/RemoveInodeMsg.h>
#include <common/storage/StorageErrors.h>
#include <storage/MetaStore.h>


class RemoveInodeMsgEx : public RemoveInodeMsg
{
   public:
      RemoveInodeMsgEx() : RemoveInodeMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats) throw(SocketException);
};

#endif /*REMOVEINODEMSGEX_H*/
