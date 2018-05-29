#ifndef RMDIRRESPMSG_H_
#define RMDIRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct RmDirRespMsg;
typedef struct RmDirRespMsg RmDirRespMsg;

static inline void RmDirRespMsg_init(RmDirRespMsg* this);
static inline void RmDirRespMsg_initFromValue(RmDirRespMsg* this, int value);

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

void RmDirRespMsg_initFromValue(RmDirRespMsg* this, int value)
{
   SimpleIntMsg_initFromValue( (SimpleIntMsg*)this, NETMSGTYPE_RmDirResp, value);
}

int RmDirRespMsg_getValue(RmDirRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}



#endif /*RMDIRRESPMSG_H_*/
