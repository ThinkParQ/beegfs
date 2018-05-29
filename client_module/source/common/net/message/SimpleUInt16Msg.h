#ifndef SIMPLEUINT16MSG_H_
#define SIMPLEUINT16MSG_H_

#include "NetMessage.h"

struct SimpleUInt16Msg;
typedef struct SimpleUInt16Msg SimpleUInt16Msg;

static inline void SimpleUInt16Msg_init(SimpleUInt16Msg* this, unsigned short msgType);
static inline void SimpleUInt16Msg_initFromValue(SimpleUInt16Msg* this, unsigned short msgType,
   uint16_t value);

// virtual functions
extern void SimpleUInt16Msg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool SimpleUInt16Msg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters & setters
static inline uint16_t SimpleUInt16Msg_getValue(SimpleUInt16Msg* this);


struct SimpleUInt16Msg
{
   NetMessage netMessage;

   uint16_t value;
};

extern const struct NetMessageOps SimpleUInt16Msg_Ops;

void SimpleUInt16Msg_init(SimpleUInt16Msg* this, unsigned short msgType)
{
   NetMessage_init(&this->netMessage, msgType, &SimpleUInt16Msg_Ops);
}

void SimpleUInt16Msg_initFromValue(SimpleUInt16Msg* this, unsigned short msgType, uint16_t value)
{
   SimpleUInt16Msg_init(this, msgType);

   this->value = value;
}

uint16_t SimpleUInt16Msg_getValue(SimpleUInt16Msg* this)
{
   return this->value;
}


#endif /* SIMPLEUINT16MSG_H_ */
