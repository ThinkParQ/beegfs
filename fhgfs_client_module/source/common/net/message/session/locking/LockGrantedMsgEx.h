#ifndef LOCKGRANTEDMSGEX_H_
#define LOCKGRANTEDMSGEX_H_

#include <common/net/message/NetMessage.h>


/**
 * This message is for deserialization (incoming) only, serialization is not implemented!
 */


struct LockGrantedMsgEx;
typedef struct LockGrantedMsgEx LockGrantedMsgEx;

static inline void LockGrantedMsgEx_init(LockGrantedMsgEx* this);

// virtual functions
extern bool LockGrantedMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx);
extern bool __LockGrantedMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);

// getters & setters
static inline const char* LockGrantedMsgEx_getLockAckID(LockGrantedMsgEx* this);
static inline const char* LockGrantedMsgEx_getAckID(LockGrantedMsgEx* this);
static inline NumNodeID LockGrantedMsgEx_getGranterNodeID(LockGrantedMsgEx* this);


struct LockGrantedMsgEx
{
   NetMessage netMessage;

   unsigned lockAckIDLen;
   const char* lockAckID;
   unsigned ackIDLen;
   const char* ackID;
   NumNodeID granterNodeID;
};

extern const struct NetMessageOps LockGrantedMsgEx_Ops;

void LockGrantedMsgEx_init(LockGrantedMsgEx* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_LockGranted, &LockGrantedMsgEx_Ops);
}

const char* LockGrantedMsgEx_getLockAckID(LockGrantedMsgEx* this)
{
   return this->lockAckID;
}

const char* LockGrantedMsgEx_getAckID(LockGrantedMsgEx* this)
{
   return this->ackID;
}

NumNodeID LockGrantedMsgEx_getGranterNodeID(LockGrantedMsgEx* this)
{
   return this->granterNodeID;
}


#endif /* LOCKGRANTEDMSGEX_H_ */
