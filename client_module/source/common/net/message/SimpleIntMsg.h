#ifndef SIMPLEINTMSG_H_
#define SIMPLEINTMSG_H_

#include "NetMessage.h"

struct SimpleIntMsg;
typedef struct SimpleIntMsg SimpleIntMsg;

static inline void SimpleIntMsg_init(SimpleIntMsg* this, unsigned short msgType);
static inline void SimpleIntMsg_initFromValue(SimpleIntMsg* this, unsigned short msgType,
   int value);

// virtual functions
extern void SimpleIntMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool SimpleIntMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters & setters
static inline int SimpleIntMsg_getValue(SimpleIntMsg* this);


struct SimpleIntMsg
{
   NetMessage netMessage;

   int value;
};

extern const struct NetMessageOps SimpleIntMsg_Ops;

void SimpleIntMsg_init(SimpleIntMsg* this, unsigned short msgType)
{
   NetMessage_init(&this->netMessage, msgType, &SimpleIntMsg_Ops);
}

void SimpleIntMsg_initFromValue(SimpleIntMsg* this, unsigned short msgType, int value)
{
   SimpleIntMsg_init(this, msgType);

   this->value = value;
}

int SimpleIntMsg_getValue(SimpleIntMsg* this)
{
   return this->value;
}


#endif /*SIMPLEINTMSG_H_*/
