#ifndef GETSTATESANDBUDDYGROUPSMSG_H_
#define GETSTATESANDBUDDYGROUPSMSG_H_

#include <common/net/message/NetMessage.h>
#include "common/nodes/NumNodeID.h"
#include <common/nodes/Node.h>

struct GetStatesAndBuddyGroupsMsg;
typedef struct GetStatesAndBuddyGroupsMsg GetStatesAndBuddyGroupsMsg;


static inline void GetStatesAndBuddyGroupsMsg_init(GetStatesAndBuddyGroupsMsg* this,
   NodeType nodeType, NumNodeID requestedByClientID);


struct GetStatesAndBuddyGroupsMsg
{
   NetMessage netMessage;

   NodeType nodeType;
   NumNodeID requestedByClientID;
};

extern const struct NetMessageOps GetStatesAndBuddyGroupsMsg_Ops;

void GetStatesAndBuddyGroupsMsg_init(GetStatesAndBuddyGroupsMsg* this, NodeType nodeType, NumNodeID requestedByClientID)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetStatesAndBuddyGroups, &GetStatesAndBuddyGroupsMsg_Ops);

   this->nodeType = nodeType;
   this->requestedByClientID = requestedByClientID;
}

#endif /* GETSTATESANDBUDDYGROUPSMSG_H_ */
