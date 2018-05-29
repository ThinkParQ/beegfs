#ifndef CHANGETARGETCONSISTENCYSTATESMSGEX_H
#define CHANGETARGETCONSISTENCYSTATESMSGEX_H

#include <common/net/message/nodes/ChangeTargetConsistencyStatesMsg.h>

class ChangeTargetConsistencyStatesMsgEx : public ChangeTargetConsistencyStatesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* CHANGETARGETSTATESMSGEX_H */
