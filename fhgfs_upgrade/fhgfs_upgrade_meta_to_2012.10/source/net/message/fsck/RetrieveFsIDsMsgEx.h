#ifndef RETRIEVEFSIDSMSGEX_H
#define RETRIEVEFSIDSMSGEX_H

#include <common/net/message/fsck/RetrieveFsIDsMsg.h>
#include <common/net/message/fsck/RetrieveFsIDsRespMsg.h>

class RetrieveFsIDsMsgEx : public RetrieveFsIDsMsg
{
   public:
      RetrieveFsIDsMsgEx() : RetrieveFsIDsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:
      
   private:
};

#endif /*RETRIEVEFSIDSMSGEX_H*/
