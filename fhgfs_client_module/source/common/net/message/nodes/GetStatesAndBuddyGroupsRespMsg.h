#ifndef GETSTATESANDBUDDYGROUPSRESPMSG_H_
#define GETSTATESANDBUDDYGROUPSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>


struct GetStatesAndBuddyGroupsRespMsg;
typedef struct GetStatesAndBuddyGroupsRespMsg GetStatesAndBuddyGroupsRespMsg;

static inline void GetStatesAndBuddyGroupsRespMsg_init(GetStatesAndBuddyGroupsRespMsg* this);

// virtual functions
extern bool GetStatesAndBuddyGroupsRespMsg_deserializePayload(NetMessage* this,
   DeserializeCtx* ctx);

// inliners
static inline void GetStatesAndBuddyGroupsRespMsg_parseBuddyGroupIDs(
   GetStatesAndBuddyGroupsRespMsg* this, UInt16List* outGroupIDs);
static inline void GetStatesAndBuddyGroupsRespMsg_parsePrimaryTargetIDs(
   GetStatesAndBuddyGroupsRespMsg* this, UInt16List* outPrimaryTargetIDs);
static inline void GetStatesAndBuddyGroupsRespMsg_parseSecondaryTargetIDs(
   GetStatesAndBuddyGroupsRespMsg* this, UInt16List* outSecondaryTargetIDs);
static inline void GetStatesAndBuddyGroupsRespMsg_parseTargetIDs(
   GetStatesAndBuddyGroupsRespMsg* this, UInt16List* outTargetIDs);
static inline void GetStatesAndBuddyGroupsRespMsg_parseReachabilityStates(
   GetStatesAndBuddyGroupsRespMsg* this, UInt8List* outReachabilityStates);
static inline void GetStatesAndBuddyGroupsRespMsg_parseConsistencyStates(
   GetStatesAndBuddyGroupsRespMsg* this, UInt8List* outConsistencyStates);

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

   // for deserialization
   RawList buddyGroupIDsList;
   RawList primaryTargetIDsList;
   RawList secondaryTargetIDsList;
   RawList targetIDsList;
   RawList targetReachabilityStates;
   RawList targetConsistencyStates;
};
extern const struct NetMessageOps GetStatesAndBuddyGroupsRespMsg_Ops;

void GetStatesAndBuddyGroupsRespMsg_init(GetStatesAndBuddyGroupsRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetStatesAndBuddyGroupsResp,
      &GetStatesAndBuddyGroupsRespMsg_Ops);
}

void GetStatesAndBuddyGroupsRespMsg_parseBuddyGroupIDs(GetStatesAndBuddyGroupsRespMsg* this,
   UInt16List* outGroupIDs)
{
   Serialization_deserializeUInt16List(&this->buddyGroupIDsList, outGroupIDs);
}

void GetStatesAndBuddyGroupsRespMsg_parsePrimaryTargetIDs(GetStatesAndBuddyGroupsRespMsg* this,
   UInt16List* outPrimaryTargetIDs)
{
   Serialization_deserializeUInt16List(&this->primaryTargetIDsList, outPrimaryTargetIDs);
}

void GetStatesAndBuddyGroupsRespMsg_parseSecondaryTargetIDs(GetStatesAndBuddyGroupsRespMsg* this,
   UInt16List* outSecondaryTargetIDs)
{
   Serialization_deserializeUInt16List(&this->secondaryTargetIDsList, outSecondaryTargetIDs);
}

void GetStatesAndBuddyGroupsRespMsg_parseTargetIDs(GetStatesAndBuddyGroupsRespMsg* this,
   UInt16List* outTargetIDs)
{
   Serialization_deserializeUInt16List(&this->targetIDsList, outTargetIDs);
}

void GetStatesAndBuddyGroupsRespMsg_parseReachabilityStates(GetStatesAndBuddyGroupsRespMsg* this,
   UInt8List* outReachabilityStates)
{
   Serialization_deserializeUInt8List(&this->targetReachabilityStates, outReachabilityStates);
}

void GetStatesAndBuddyGroupsRespMsg_parseConsistencyStates(GetStatesAndBuddyGroupsRespMsg* this,
   UInt8List* outConsistencyStates)
{
   Serialization_deserializeUInt8List(&this->targetConsistencyStates, outConsistencyStates);
}

#endif /* GETSTATESANDBUDDYGROUPSRESPMSG_H_ */
