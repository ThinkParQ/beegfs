#ifndef SETROOTNODEIDMSGEX_H
#define SETROOTNODEIDMSGEX_H

#include <common/net/message/nodes/SetRootNodeIDMsg.h>

class SetRootNodeIDMsgEx : public SetRootNodeIDMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*SETROOTNODEIDMSGEX_H*/
