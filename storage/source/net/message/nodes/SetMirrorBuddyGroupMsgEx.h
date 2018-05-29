#ifndef SETMIRRORBUDDYGROUPMSGEX_H_
#define SETMIRRORBUDDYGROUPMSGEX_H_

#include <common/net/message/nodes/SetMirrorBuddyGroupMsg.h>

class SetMirrorBuddyGroupMsgEx : public SetMirrorBuddyGroupMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* SETMIRRORBUDDYGROUPMSGEX_H_ */
