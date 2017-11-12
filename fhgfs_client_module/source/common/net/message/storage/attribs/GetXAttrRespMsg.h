#ifndef GETXATTRRESPMSG_H_
#define GETXATTRRESPMSG_H_

#include <common/net/message/NetMessage.h>

struct GetXAttrRespMsg;
typedef struct GetXAttrRespMsg GetXAttrRespMsg;

static inline void GetXAttrRespMsg_init(GetXAttrRespMsg* this);

// virtual functions
extern bool GetXAttrRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters and setters
static inline char* GetXAttrRespMsg_getValueBuf(GetXAttrRespMsg* this);
static inline int GetXAttrRespMsg_getReturnCode(GetXAttrRespMsg* this);
static inline int GetXAttrRespMsg_getSize(GetXAttrRespMsg* this);

struct GetXAttrRespMsg
{
   NetMessage netMessage;

   unsigned valueBufLen; // only used for deserialization
   char* valueBuf;
   int size;
   int returnCode;
};

extern const struct NetMessageOps GetXAttrRespMsg_Ops;

void GetXAttrRespMsg_init(GetXAttrRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetXAttrResp, &GetXAttrRespMsg_Ops);
}

char* GetXAttrRespMsg_getValueBuf(GetXAttrRespMsg* this)
{
   return this->valueBuf;
}

int GetXAttrRespMsg_getReturnCode(GetXAttrRespMsg* this)
{
   return this->returnCode;
}

int GetXAttrRespMsg_getSize(GetXAttrRespMsg* this)
{
   return this->size;
}

#endif /*GETXATTRRESPMSG_H_*/
