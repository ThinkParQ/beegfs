#ifndef RETRIEVEINODESMSGEX_H
#define RETRIEVEINODESMSGEX_H

#include <common/net/message/fsck/RetrieveInodesMsg.h>
#include <common/net/message/fsck/RetrieveInodesRespMsg.h>

class RetrieveInodesMsgEx : public RetrieveInodesMsg
{
   public:
      RetrieveInodesMsgEx() : RetrieveInodesMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:
      
   private:
};

#endif /*RETRIEVEINODESMSGEX_H*/
