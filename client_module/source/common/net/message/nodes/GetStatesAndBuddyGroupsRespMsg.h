#ifndef GETSTATESANDBUDDYGROUPSRESPMSG_H_
#define GETSTATESANDBUDDYGROUPSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>
#include <common/Types.h>


struct GetStatesAndBuddyGroupsRespMsg;
typedef struct GetStatesAndBuddyGroupsRespMsg GetStatesAndBuddyGroupsRespMsg;

static inline void GetStatesAndBuddyGroupsRespMsg_init(GetStatesAndBuddyGroupsRespMsg* this);

// virtual functions
extern bool GetStatesAndBuddyGroupsRespMsg_deserializePayload(NetMessage* this,
   DeserializeCtx* ctx);

/**
 * This message carries two maps:
 *    1) buddyGroupID -> primaryTarget, secondaryTarget
 *    2) targetID -> targetReachabilityState, targetConsistencyState
 *
 * Note: This message can only be received/deserialized (send/serialization not implemented).
 */
struct GetStatesAndBuddyGroupsRespMsg
{
   NetMessage netMessage;

   struct list_head groups; /* struct BuddyGroupMapping */
   struct list_head states; /* struct TargetStateMapping */
};
extern const struct NetMessageOps GetStatesAndBuddyGroupsRespMsg_Ops;

void GetStatesAndBuddyGroupsRespMsg_init(GetStatesAndBuddyGroupsRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetStatesAndBuddyGroupsResp,
      &GetStatesAndBuddyGroupsRespMsg_Ops);

   INIT_LIST_HEAD(&this->groups);
   INIT_LIST_HEAD(&this->states);
}

#endif /* GETSTATESANDBUDDYGROUPSRESPMSG_H_ */
