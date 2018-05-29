#ifndef REFRESHTARGETSTATESMSGEX_H_
#define REFRESHTARGETSTATESMSGEX_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

/**
 * Note: Only the receive/deserialize part of this message is implemented (so it cannot be sent).
 * Note: Processing only sends response when ackID is set (no RefreshTargetStatesRespMsg
 * implemented).
 */

struct RefreshTargetStatesMsgEx;
typedef struct RefreshTargetStatesMsgEx RefreshTargetStatesMsgEx;

static inline void RefreshTargetStatesMsgEx_init(RefreshTargetStatesMsgEx* this);

// virtual functions
extern bool RefreshTargetStatesMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx);
extern bool __RefreshTargetStatesMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);

// getters & setters
static inline const char* RefreshTargetStatesMsgEx_getAckID(RefreshTargetStatesMsgEx* this);


struct RefreshTargetStatesMsgEx
{
   NetMessage netMessage;

   unsigned ackIDLen;
   const char* ackID;
};

extern const struct NetMessageOps RefreshTargetStatesMsgEx_Ops;

void RefreshTargetStatesMsgEx_init(RefreshTargetStatesMsgEx* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_RefreshTargetStates, &RefreshTargetStatesMsgEx_Ops);
}

const char* RefreshTargetStatesMsgEx_getAckID(RefreshTargetStatesMsgEx* this)
{
   return this->ackID;
}

#endif /* REFRESHTARGETSTATESMSGEX_H_ */
