#ifndef REFRESHCAPACITYPOOLSMSGEX_H_
#define REFRESHCAPACITYPOOLSMSGEX_H_

#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>


class RefreshCapacityPoolsMsgEx : public RefreshCapacityPoolsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* REFRESHCAPACITYPOOLSMSGEX_H_ */
