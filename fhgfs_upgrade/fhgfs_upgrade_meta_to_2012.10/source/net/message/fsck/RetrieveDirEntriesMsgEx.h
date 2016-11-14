#ifndef RETRIEVEDIRENTRIESMSGEX_H
#define RETRIEVEDIRENTRIESMSGEX_H

#include <common/net/message/fsck/RetrieveDirEntriesMsg.h>
#include <common/net/message/fsck/RetrieveDirEntriesRespMsg.h>

class RetrieveDirEntriesMsgEx : public RetrieveDirEntriesMsg
{
   public:
      RetrieveDirEntriesMsgEx() : RetrieveDirEntriesMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:
      
   private:
};

#endif /*RETRIEVEDIRENTRIESMSGEX_H*/
