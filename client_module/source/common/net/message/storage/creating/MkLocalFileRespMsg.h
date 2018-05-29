#ifndef MKLOCALFILERESPMSG_H_
#define MKLOCALFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct MkLocalFileRespMsg;
typedef struct MkLocalFileRespMsg MkLocalFileRespMsg;

static inline void MkLocalFileRespMsg_init(MkLocalFileRespMsg* this);
static inline void MkLocalFileRespMsg_initFromValue(MkLocalFileRespMsg* this, int value);

// getters & setters
static inline int MkLocalFileRespMsg_getValue(MkLocalFileRespMsg* this);


struct MkLocalFileRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void MkLocalFileRespMsg_init(MkLocalFileRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_MkLocalFileResp);
}

void MkLocalFileRespMsg_initFromValue(MkLocalFileRespMsg* this, int value)
{
   SimpleIntMsg_initFromValue( (SimpleIntMsg*)this, NETMSGTYPE_MkLocalFileResp, value);
}

int MkLocalFileRespMsg_getValue(MkLocalFileRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}



#endif /*MKLOCALFILERESPMSG_H_*/
