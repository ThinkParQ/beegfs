#ifndef SETFILESTATERESPMSG_H_
#define SETFILESTATERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

struct SetFileStateRespMsg;
typedef struct SetFileStateRespMsg SetFileStateRespMsg;

static inline void SetFileStateRespMsg_init(SetFileStateRespMsg* this);

struct SetFileStateRespMsg
{
   SimpleIntMsg simpleIntMsg;
};

void SetFileStateRespMsg_init(SetFileStateRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_SetFileStateResp);
}

// getters & setters
static inline int SetFileStateRespMsg_getValue(SetFileStateRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}

#endif /*SETFILESTATERESPMSG_H_*/
