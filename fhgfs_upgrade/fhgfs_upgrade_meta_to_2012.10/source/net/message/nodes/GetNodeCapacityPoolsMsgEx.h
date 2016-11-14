#ifndef GETNODECAPACITYPOOLSMSGEX_H_
#define GETNODECAPACITYPOOLSMSGEX_H_

#include <common/net/message/nodes/GetNodeCapacityPoolsMsg.h>

class GetNodeCapacityPoolsMsgEx : public GetNodeCapacityPoolsMsg
{
   public:
      GetNodeCapacityPoolsMsgEx() : GetNodeCapacityPoolsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:

   private:

};

#endif /* GETNODECAPACITYPOOLSMSGEX_H_ */
