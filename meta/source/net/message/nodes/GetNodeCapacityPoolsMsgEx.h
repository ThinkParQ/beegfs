#ifndef GETNODECAPACITYPOOLSMSGEX_H_
#define GETNODECAPACITYPOOLSMSGEX_H_

#include <common/net/message/nodes/GetNodeCapacityPoolsMsg.h>

class GetNodeCapacityPoolsMsgEx : public GetNodeCapacityPoolsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* GETNODECAPACITYPOOLSMSGEX_H_ */
