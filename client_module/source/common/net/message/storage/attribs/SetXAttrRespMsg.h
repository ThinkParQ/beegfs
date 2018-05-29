#ifndef SETXATTRRESPMSG_H_
#define SETXATTRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

struct SetXAttrRespMsg;
typedef struct SetXAttrRespMsg SetXAttrRespMsg;

static inline void SetXAttrRespMsg_init(SetXAttrRespMsg* this);

struct SetXAttrRespMsg
{
   SimpleIntMsg simpleIntMsg;
};

void SetXAttrRespMsg_init(SetXAttrRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_SetXAttrResp);
}

// getters & setters
static inline int SetXAttrRespMsg_getValue(SetXAttrRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}

#endif /*SETXATTRRESPMSG_H_*/
