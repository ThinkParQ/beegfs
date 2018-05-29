#ifndef SETTARGETCONSISTENCYSTATESMSGEX_H
#define SETTARGETCONSISTENCYSTATESMSGEX_H

#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>

class SetTargetConsistencyStatesMsgEx : public SetTargetConsistencyStatesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* SETTARGETCONSISTENCYSTATESMSGEX_H */
