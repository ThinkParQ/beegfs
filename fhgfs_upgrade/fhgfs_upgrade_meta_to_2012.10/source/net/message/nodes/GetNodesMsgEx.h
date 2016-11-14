#ifndef GETNODESMSGEX_H_
#define GETNODESMSGEX_H_

#include <common/net/message/nodes/GetNodesMsg.h>

class GetNodesMsgEx : public GetNodesMsg
{
   public:
      GetNodesMsgEx() : GetNodesMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:
      
   private:

};

#endif /*GETNODESMSGEX_H_*/
