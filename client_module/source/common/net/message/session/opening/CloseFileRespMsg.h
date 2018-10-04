#ifndef CLOSEFILERESPMSG_H_
#define CLOSEFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct CloseFileRespMsg;
typedef struct CloseFileRespMsg CloseFileRespMsg;

static inline void CloseFileRespMsg_init(CloseFileRespMsg* this);

// getters & setters
static inline int CloseFileRespMsg_getValue(CloseFileRespMsg* this);

struct CloseFileRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void CloseFileRespMsg_init(CloseFileRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_CloseFileResp);
}

int CloseFileRespMsg_getValue(CloseFileRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}


#endif /*CLOSEFILERESPMSG_H_*/
