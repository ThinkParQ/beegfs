#ifndef SETMIRRORBUDDYGROUPMSGEX_H_
#define SETMIRRORBUDDYGROUPMSGEX_H_

#include <common/net/message/NetMessage.h>


struct SetMirrorBuddyGroupMsgEx;
typedef struct SetMirrorBuddyGroupMsgEx SetMirrorBuddyGroupMsgEx;

static inline void SetMirrorBuddyGroupMsgEx_init(SetMirrorBuddyGroupMsgEx* this);

// virtual functions
extern bool SetMirrorBuddyGroupMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx);
extern bool __SetMirrorBuddyGroupMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);

// getters and setters
static inline NodeType SetMirrorBuddyGroupMsgEx_getNodeType(SetMirrorBuddyGroupMsgEx* this);
static inline uint16_t SetMirrorBuddyGroupMsgEx_getPrimaryTargetID(SetMirrorBuddyGroupMsgEx* this);
static inline uint16_t SetMirrorBuddyGroupMsgEx_getSecondaryTargetID(SetMirrorBuddyGroupMsgEx* this);
static inline uint16_t SetMirrorBuddyGroupMsgEx_getBuddyGroupID(SetMirrorBuddyGroupMsgEx* this);
static inline bool SetMirrorBuddyGroupMsgEx_getAllowUpdate(SetMirrorBuddyGroupMsgEx* this);
static inline const char* SetMirrorBuddyGroupMsgEx_getAckID(SetMirrorBuddyGroupMsgEx* this);

struct SetMirrorBuddyGroupMsgEx
{
   NetMessage netMessage;

   int nodeType;
   uint16_t primaryTargetID;
   uint16_t secondaryTargetID;
   uint16_t buddyGroupID;
   bool allowUpdate;

   unsigned ackIDLen;
   const char* ackID;
};

extern const struct NetMessageOps SetMirrorBuddyGroupMsgEx_Ops;

void SetMirrorBuddyGroupMsgEx_init(SetMirrorBuddyGroupMsgEx* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_SetMirrorBuddyGroup,
      &SetMirrorBuddyGroupMsgEx_Ops);
}

static inline NodeType SetMirrorBuddyGroupMsgEx_getNodeType(SetMirrorBuddyGroupMsgEx* this)
{
   return (NodeType)this->nodeType;
}

static inline uint16_t SetMirrorBuddyGroupMsgEx_getPrimaryTargetID(SetMirrorBuddyGroupMsgEx* this)
{
   return this->primaryTargetID;
}

static inline uint16_t SetMirrorBuddyGroupMsgEx_getSecondaryTargetID(SetMirrorBuddyGroupMsgEx* this)
{
   return this->secondaryTargetID;
}

static inline uint16_t SetMirrorBuddyGroupMsgEx_getBuddyGroupID(SetMirrorBuddyGroupMsgEx* this)
{
   return this->buddyGroupID;
}

static inline bool SetMirrorBuddyGroupMsgEx_getAllowUpdate(SetMirrorBuddyGroupMsgEx* this)
{
   return this->allowUpdate;
}

const char* SetMirrorBuddyGroupMsgEx_getAckID(SetMirrorBuddyGroupMsgEx* this)
{
   return this->ackID;
}

#endif /* SETMIRRORBUDDYGROUPMSGEX_H_ */
