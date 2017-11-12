#ifndef SIMPLESTRINGMSG_H_
#define SIMPLESTRINGMSG_H_

#include "NetMessage.h"

struct SimpleStringMsg;
typedef struct SimpleStringMsg SimpleStringMsg;

static inline void SimpleStringMsg_init(SimpleStringMsg* this, unsigned short msgType);
static inline void SimpleStringMsg_initFromValue(SimpleStringMsg* this, unsigned short msgType,
   const char* value);

// virtual functions
extern void SimpleStringMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool SimpleStringMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters & setters
static inline const char* SimpleStringMsg_getValue(SimpleStringMsg* this);

struct SimpleStringMsg
{
   NetMessage netMessage;

   const char* value;
   unsigned valueLen;
};

extern const struct NetMessageOps SimpleStringMsg_Ops;

void SimpleStringMsg_init(SimpleStringMsg* this, unsigned short msgType)
{
   NetMessage_init(&this->netMessage, msgType, &SimpleStringMsg_Ops);
}

void SimpleStringMsg_initFromValue(SimpleStringMsg* this, unsigned short msgType, const char* value)
{
   SimpleStringMsg_init(this, msgType);

   this->value = value;
   this->valueLen = strlen(value);
}

const char* SimpleStringMsg_getValue(SimpleStringMsg* this)
{
   return this->value;
}

#endif /* SIMPLESTRINGMSG_H_ */
