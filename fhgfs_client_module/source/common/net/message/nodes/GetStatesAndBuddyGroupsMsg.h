#ifndef GETSTATESANDBUDDYGROUPSMSG_H_
#define GETSTATESANDBUDDYGROUPSMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct GetStatesAndBuddyGroupsMsg;
typedef struct GetStatesAndBuddyGroupsMsg GetStatesAndBuddyGroupsMsg;


static inline void GetStatesAndBuddyGroupsMsg_init(GetStatesAndBuddyGroupsMsg* this,
   NodeType nodeType);


struct GetStatesAndBuddyGroupsMsg
{
   SimpleIntMsg simpleIntMsg;
};


void GetStatesAndBuddyGroupsMsg_init(GetStatesAndBuddyGroupsMsg* this, NodeType nodeType)
{
   SimpleIntMsg_initFromValue( (SimpleIntMsg*)this, NETMSGTYPE_GetStatesAndBuddyGroups, nodeType);
}

#endif /* GETSTATESANDBUDDYGROUPSMSG_H_ */
