#ifndef FIXMETADATAOWNERMSGEX_H
#define FIXMETADATAOWNERMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/DeleteDirEntriesMsg.h>
#include <common/net/message/fsck/DeleteDirEntriesRespMsg.h>

class DeleteDirEntriesMsgEx : public DeleteDirEntriesMsg
{
   public:
      DeleteDirEntriesMsgEx() : DeleteDirEntriesMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
         size_t bufLen, HighResolutionStats* stats);

   private:

};

#endif /*FIXMETADATAOWNERMSGEX_H*/
