#ifndef GETMIRRORBUDDYGROUPSMSGEX_H_
#define GETMIRRORBUDDYGROUPSMSGEX_H_

#include <common/net/message/nodes/GetMirrorBuddyGroupsMsg.h>

class GetMirrorBuddyGroupsMsgEx : public GetMirrorBuddyGroupsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* GETMIRRORBUDDYGROUPSMSGEX_H_ */
