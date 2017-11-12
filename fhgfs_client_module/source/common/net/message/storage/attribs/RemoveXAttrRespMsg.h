#ifndef  REMOVEXATTRRESPMSG_H_
#define  REMOVEXATTRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

struct RemoveXAttrRespMsg;
typedef struct RemoveXAttrRespMsg RemoveXAttrRespMsg;

static inline void RemoveXAttrRespMsg_init(RemoveXAttrRespMsg* this);

struct RemoveXAttrRespMsg
{
   SimpleIntMsg simpleIntMsg;
};

void RemoveXAttrRespMsg_init(RemoveXAttrRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_RemoveXAttrResp);
}

// getters & setters
static inline int RemoveXAttrRespMsg_getValue(RemoveXAttrRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}

#endif /*REMOVEXATTRRESPMSG_H_*/
