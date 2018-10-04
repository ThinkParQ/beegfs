#ifndef LOGRESPMSG_H_
#define LOGRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct LogRespMsg;
typedef struct LogRespMsg LogRespMsg;

static inline void LogRespMsg_init(LogRespMsg* this);

// getters & setters
static inline int LogRespMsg_getValue(LogRespMsg* this);

struct LogRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void LogRespMsg_init(LogRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_LogResp);
}

int LogRespMsg_getValue(LogRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}


#endif /*LOGRESPMSG_H_*/
