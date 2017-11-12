#ifndef UNLINKLOCALFILERESPMSG_H_
#define UNLINKLOCALFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct UnlinkLocalFileRespMsg;
typedef struct UnlinkLocalFileRespMsg UnlinkLocalFileRespMsg;

static inline void UnlinkLocalFileRespMsg_init(UnlinkLocalFileRespMsg* this);
static inline void UnlinkLocalFileRespMsg_initFromValue(UnlinkLocalFileRespMsg* this, int value);

// getters & setters
static inline int UnlinkLocalFileRespMsg_getValue(UnlinkLocalFileRespMsg* this);

struct UnlinkLocalFileRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void UnlinkLocalFileRespMsg_init(UnlinkLocalFileRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_UnlinkLocalFileResp);
}

void UnlinkLocalFileRespMsg_initFromValue(UnlinkLocalFileRespMsg* this, int value)
{
   SimpleIntMsg_initFromValue( (SimpleIntMsg*)this, NETMSGTYPE_UnlinkLocalFileResp, value);
}

int UnlinkLocalFileRespMsg_getValue(UnlinkLocalFileRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}



#endif /*UNLINKLOCALFILERESPMSG_H_*/
