#ifndef GETTARGETSTATESMSGEX_H_
#define GETTARGETSTATESMSGEX_H_

#include <common/net/message/nodes/GetTargetStatesMsg.h>

class GetTargetStatesMsgEx : public GetTargetStatesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* GETTARGETSTATESMSGEX_H_ */
