#ifndef SIMPLEMSG_H_
#define SIMPLEMSG_H_

#include "NetMessage.h"

/**
 * Note: Simple messages are defined by the header (resp. the msgType) only and
 * require no additional data
 */

struct SimpleMsg;
typedef struct SimpleMsg SimpleMsg;

static inline void SimpleMsg_init(SimpleMsg* this, unsigned short msgType);

// virtual functions
extern void SimpleMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool SimpleMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);


struct SimpleMsg
{
   NetMessage netMessage;
};

extern const struct NetMessageOps SimpleMsg_Ops;

void SimpleMsg_init(SimpleMsg* this, unsigned short msgType)
{
   NetMessage_init(&this->netMessage, msgType, &SimpleMsg_Ops);
}

#endif /*SIMPLEMSG_H_*/
