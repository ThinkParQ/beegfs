#ifndef GETMIRRORBUDDYGROUPSMSG_H
#define GETMIRRORBUDDYGROUPSMSG_H

#include <common/net/message/SimpleIntMsg.h>

struct GetMirrorBuddyGroupsMsg;
typedef struct GetMirrorBuddyGroupsMsg GetMirrorBuddyGroupsMsg;

static inline void GetMirrorBuddyGroupsMsg_init(GetMirrorBuddyGroupsMsg* this, NodeType nodeType);

struct GetMirrorBuddyGroupsMsg
{
   SimpleIntMsg simpleIntMsg;
};

void GetMirrorBuddyGroupsMsg_init(GetMirrorBuddyGroupsMsg* this, NodeType nodeType)
{
   SimpleIntMsg_initFromValue( (SimpleIntMsg*)this, NETMSGTYPE_GetMirrorBuddyGroups, nodeType);
}

#endif /* GETMIRRORBUDDYGROUPSMSG_H */
