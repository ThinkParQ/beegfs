#pragma once

#include <common/net/message/nodes/SetMirrorBuddyGroupMsg.h>

class SetMirrorBuddyGroupMsgEx : public SetMirrorBuddyGroupMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


