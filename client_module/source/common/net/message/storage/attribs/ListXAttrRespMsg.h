#ifndef LISTXATTRRESPMSG_H_
#define LISTXATTRRESPMSG_H_

#include <common/net/message/NetMessage.h>

struct ListXAttrRespMsg;
typedef struct ListXAttrRespMsg ListXAttrRespMsg;

static inline void ListXAttrRespMsg_init(ListXAttrRespMsg* this);

// virtual functions
extern bool ListXAttrRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters and setters
static inline char* ListXAttrRespMsg_getValueBuf(ListXAttrRespMsg* this);
static inline int ListXAttrRespMsg_getReturnCode(ListXAttrRespMsg* this);
static inline int ListXAttrRespMsg_getSize(ListXAttrRespMsg* this);

struct ListXAttrRespMsg
{
   NetMessage netMessage;

   unsigned valueElemNum;
   char* valueBuf;
   int size;
   int returnCode;
};

extern const struct NetMessageOps ListXAttrRespMsg_Ops;

void ListXAttrRespMsg_init(ListXAttrRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_ListXAttrResp, &ListXAttrRespMsg_Ops);
}

char* ListXAttrRespMsg_getValueBuf(ListXAttrRespMsg* this)
{
   return this->valueBuf;
}

int ListXAttrRespMsg_getReturnCode(ListXAttrRespMsg* this)
{
   return this->returnCode;
}

int ListXAttrRespMsg_getSize(ListXAttrRespMsg* this)
{
   return this->size;
}

#endif /*LISTXATTRRESPMSG_H_*/
