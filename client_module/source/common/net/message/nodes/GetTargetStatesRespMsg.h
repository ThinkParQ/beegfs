#ifndef GETTARGETSTATESRESPMSG_H
#define GETTARGETSTATESRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

/**
 * Note: This message can only be received/deserialized (send/serialization not implemented)
 */

struct GetTargetStatesRespMsg;
typedef struct GetTargetStatesRespMsg GetTargetStatesRespMsg;

static inline void GetTargetStatesRespMsg_init(GetTargetStatesRespMsg* this);

// virtual functions
extern bool GetTargetStatesRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// inliners
static inline void GetTargetStatesRespMsg_parseTargetIDs(GetTargetStatesRespMsg* this,
   UInt16List* outTargetIDs);
static inline void GetTargetStatesRespMsg_parseReachabilityStates(
   GetTargetStatesRespMsg* this, UInt8List* outReachabilityStates);
static inline void GetTargetStatesRespMsg_parseConsistencyStates(
   GetTargetStatesRespMsg* this, UInt8List* outConsistencyStates);

struct GetTargetStatesRespMsg
{
   NetMessage netMessage;

   /*
    * NOTE: UInt8List is used here for target states; must be consistent with userspace common lib
    */

   // for serialization
   UInt16List* targetIDs; // not owned by this object!
   UInt8List* reachabilityStates; // not owned by this object!
   UInt8List* consistencyStates; // not owned by this object!

   // for deserialization
   RawList rawTargetIDsList;
   RawList rawReachabilityStates;
   RawList rawConsistencyStates;
};

extern const struct NetMessageOps GetTargetStatesRespMsg_Ops;

void GetTargetStatesRespMsg_init(GetTargetStatesRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetTargetStatesResp, &GetTargetStatesRespMsg_Ops);
}

void GetTargetStatesRespMsg_parseTargetIDs(GetTargetStatesRespMsg* this, UInt16List* outTargetIDs)
{
   Serialization_deserializeUInt16List(&this->rawTargetIDsList, outTargetIDs);
}

void GetTargetStatesRespMsg_parseReachabilityStates(GetTargetStatesRespMsg* this,
   UInt8List* outReachabilityStates)
{
   Serialization_deserializeUInt8List(&this->rawReachabilityStates, outReachabilityStates);
}

void GetTargetStatesRespMsg_parseConsistencyStates(GetTargetStatesRespMsg* this,
   UInt8List* outConsistencyStates)
{
   Serialization_deserializeUInt8List(&this->rawConsistencyStates, outConsistencyStates);
}

#endif /* GETTARGETSTATESRESPMSG_H */
