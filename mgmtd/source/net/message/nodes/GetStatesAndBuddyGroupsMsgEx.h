#ifndef GETSTATESANDBUDDYGROUPSMSGEX_H_
#define GETSTATESANDBUDDYGROUPSMSGEX_H_

#include <common/net/message/nodes/GetStatesAndBuddyGroupsMsg.h>

class GetStatesAndBuddyGroupsMsgEx : public GetStatesAndBuddyGroupsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*GETSTATESANDBUDDYGROUPSMSGEX_H_*/
