#ifndef SIMPLEINT64MSG_H_
#define SIMPLEINT64MSG_H_

#include "NetMessage.h"

struct SimpleInt64Msg;
typedef struct SimpleInt64Msg SimpleInt64Msg;

static inline void SimpleInt64Msg_init(SimpleInt64Msg* this, unsigned short msgType);
static inline void SimpleInt64Msg_initFromValue(SimpleInt64Msg* this, unsigned short msgType,
   int64_t value);

// virtual functions
extern void SimpleInt64Msg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool SimpleInt64Msg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters & setters
static inline int64_t SimpleInt64Msg_getValue(SimpleInt64Msg* this);

struct SimpleInt64Msg
{
   NetMessage netMessage;

   int64_t value;
};

extern const struct NetMessageOps SimpleInt64Msg_Ops;

void SimpleInt64Msg_init(SimpleInt64Msg* this, unsigned short msgType)
{
   NetMessage_init(&this->netMessage, msgType, &SimpleInt64Msg_Ops);
}

void SimpleInt64Msg_initFromValue(SimpleInt64Msg* this, unsigned short msgType, int64_t value)
{
   SimpleInt64Msg_init(this, msgType);

   this->value = value;
}

int64_t SimpleInt64Msg_getValue(SimpleInt64Msg* this)
{
   return this->value;
}

#endif /*SIMPLEINT64MSG_H_*/
