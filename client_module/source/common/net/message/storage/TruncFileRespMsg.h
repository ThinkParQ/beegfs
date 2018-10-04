#ifndef TRUNCFILERESPMSG_H_
#define TRUNCFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct TruncFileRespMsg;
typedef struct TruncFileRespMsg TruncFileRespMsg;

static inline void TruncFileRespMsg_init(TruncFileRespMsg* this);

// getters & setters
static inline int TruncFileRespMsg_getValue(TruncFileRespMsg* this);

struct TruncFileRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void TruncFileRespMsg_init(TruncFileRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_TruncFileResp);
}

int TruncFileRespMsg_getValue(TruncFileRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}



#endif /*TRUNCFILERESPMSG_H_*/
