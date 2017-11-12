#ifndef SIMPLEINTSTRINGMSG_H_
#define SIMPLEINTSTRINGMSG_H_

#include "NetMessage.h"

struct SimpleIntStringMsg;
typedef struct SimpleIntStringMsg SimpleIntStringMsg;

static inline void SimpleIntStringMsg_init(SimpleIntStringMsg* this, unsigned short msgType);
static inline void SimpleIntStringMsg_initFromValue(SimpleIntStringMsg* this, unsigned short msgType,
   int intValue, const char* strValue);

// virtual functions
extern void SimpleIntStringMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool SimpleIntStringMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters & setters
static inline int SimpleIntStringMsg_getIntValue(SimpleIntStringMsg* this);
static inline const char* SimpleIntStringMsg_getStrValue(SimpleIntStringMsg* this);


/**
 * Simple message containing an integer value and a string (e.g. int error code and human-readable
 * explantion with more details as string).
 */
struct SimpleIntStringMsg
{
   NetMessage netMessage;

   int intValue;

   const char* strValue;
   unsigned strValueLen;
};

extern const struct NetMessageOps SimpleIntStringMsg_Ops;

void SimpleIntStringMsg_init(SimpleIntStringMsg* this, unsigned short msgType)
{
   NetMessage_init(&this->netMessage, msgType, &SimpleIntStringMsg_Ops);
}

/**
 * @param strValue just a reference, so don't free or modify it while this msg is used.
 */
void SimpleIntStringMsg_initFromValue(SimpleIntStringMsg* this, unsigned short msgType,
   int intValue, const char* strValue)
{
   SimpleIntStringMsg_init(this, msgType);

   this->intValue = intValue;

   this->strValue = strValue;
   this->strValueLen = strlen(strValue);
}

int SimpleIntStringMsg_getIntValue(SimpleIntStringMsg* this)
{
   return this->intValue;
}

const char* SimpleIntStringMsg_getStrValue(SimpleIntStringMsg* this)
{
   return this->strValue;
}



#endif /* SIMPLEINTSTRINGMSG_H_ */
