#ifndef STORAGE_GETTARGETCONSISTENCYSTATESMSGEX_H
#define STORAGE_GETTARGETCONSISTENCYSTATESMSGEX_H

#include <common/net/message/nodes/GetTargetConsistencyStatesMsg.h>

class GetTargetConsistencyStatesMsgEx : public GetTargetConsistencyStatesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* STORAGE_GETTARGETCONSISTENCYSTATESMSGEX_H */
