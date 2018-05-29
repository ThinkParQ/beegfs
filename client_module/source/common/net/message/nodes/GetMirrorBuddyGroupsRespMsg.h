#ifndef GETMIRRORBUDDYGROUPSRESPMSG_H
#define GETMIRRORBUDDYGROUPSRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

/**
 * Note: This message can only be received/deserialized (send/serialization not implemented)
 */

struct GetMirrorBuddyGroupsRespMsg;
typedef struct GetMirrorBuddyGroupsRespMsg GetMirrorBuddyGroupsRespMsg;

static inline void GetMirrorBuddyGroupsRespMsg_init(GetMirrorBuddyGroupsRespMsg* this);

// virtual functions
extern bool GetMirrorBuddyGroupsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// inliners
static inline void GetMirrorBuddyGroupsRespMsg_parseBuddyGroupIDs(GetMirrorBuddyGroupsRespMsg* this,
   UInt16List* outGroupIDs);
static inline void GetMirrorBuddyGroupsRespMsg_parsePrimaryTargetIDs(
   GetMirrorBuddyGroupsRespMsg* this, UInt16List* outPrimaryTargetIDs);
static inline void GetMirrorBuddyGroupsRespMsg_parseSecondaryTargetIDs(
   GetMirrorBuddyGroupsRespMsg* this, UInt16List* outSecondaryTargetIDs);

struct GetMirrorBuddyGroupsRespMsg
{
   NetMessage netMessage;

   // for serialization
   UInt16List* mirrorBuddyGroupIDs; // not owned by this object!
   UInt16List* primaryTargetIDs; // not owned by this object!
   UInt16List* secondaryTargetIDs; // not owned by this object!

   // for deserialization
   RawList rawBuddyGroupIDsList;
   RawList rawPrimaryTargetIDsList;
   RawList rawSecondaryTargetIDsList;
};

extern const struct NetMessageOps GetMirrorBuddyGroupsRespMsg_Ops;

void GetMirrorBuddyGroupsRespMsg_init(GetMirrorBuddyGroupsRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetMirrorBuddyGroupsResp,
      &GetMirrorBuddyGroupsRespMsg_Ops);
}

void GetMirrorBuddyGroupsRespMsg_parseBuddyGroupIDs(GetMirrorBuddyGroupsRespMsg* this,
   UInt16List* outGroupIDs)
{
   Serialization_deserializeUInt16List(&this->rawBuddyGroupIDsList, outGroupIDs);
}

void GetMirrorBuddyGroupsRespMsg_parsePrimaryTargetIDs(GetMirrorBuddyGroupsRespMsg* this,
   UInt16List* outPrimaryTargetIDs)
{
   Serialization_deserializeUInt16List(&this->rawPrimaryTargetIDsList, outPrimaryTargetIDs);
}

void GetMirrorBuddyGroupsRespMsg_parseSecondaryTargetIDs(GetMirrorBuddyGroupsRespMsg* this,
   UInt16List* outSecondaryTargetIDs)
{
   Serialization_deserializeUInt16List(&this->rawSecondaryTargetIDsList, outSecondaryTargetIDs);
}

#endif /* GETMIRRORBUDDYGROUPSRESPMSG_H */
