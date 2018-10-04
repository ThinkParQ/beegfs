#ifndef RMDIRRESPMSG_H_
#define RMDIRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct RmDirRespMsg;
typedef struct RmDirRespMsg RmDirRespMsg;

static inline void RmDirRespMsg_init(RmDirRespMsg* this);

// getters & setters
static inline int RmDirRespMsg_getValue(RmDirRespMsg* this);

struct RmDirRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void RmDirRespMsg_init(RmDirRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_RmDirResp);
}

int RmDirRespMsg_getValue(RmDirRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}



#endif /*RMDIRRESPMSG_H_*/
