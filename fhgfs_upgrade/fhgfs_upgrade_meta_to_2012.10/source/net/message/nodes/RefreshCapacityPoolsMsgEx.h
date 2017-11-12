#ifndef REFRESHCAPACITYPOOLSMSGEX_H_
#define REFRESHCAPACITYPOOLSMSGEX_H_

#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>


class RefreshCapacityPoolsMsgEx : public RefreshCapacityPoolsMsg
{
   public:
      RefreshCapacityPoolsMsgEx() : RefreshCapacityPoolsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:
};

#endif /* REFRESHCAPACITYPOOLSMSGEX_H_ */
