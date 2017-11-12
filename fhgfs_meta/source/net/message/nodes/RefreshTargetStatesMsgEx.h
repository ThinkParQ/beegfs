#ifndef REFRESHTARGETSTATESMSGEX_H_
#define REFRESHTARGETSTATESMSGEX_H_

#include <common/net/message/nodes/RefreshTargetStatesMsg.h>


class RefreshTargetStatesMsgEx : public RefreshTargetStatesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* REFRESHTARGETSTATESMSGEX_H_ */
