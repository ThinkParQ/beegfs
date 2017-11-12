#ifndef REMOVEBUDDYGROUPMSGEX_H
#define REMOVEBUDDYGROUPMSGEX_H

#include <common/net/message/nodes/RemoveBuddyGroupMsg.h>

class RemoveBuddyGroupMsgEx : public RemoveBuddyGroupMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif
