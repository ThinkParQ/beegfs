#ifndef HARDLINKRESPMSG_H_
#define HARDLINKRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct HardlinkRespMsg;
typedef struct HardlinkRespMsg HardlinkRespMsg;

static inline void HardlinkRespMsg_init(HardlinkRespMsg* this);

// getters & setters
static inline int HardlinkRespMsg_getValue(HardlinkRespMsg* this);

struct HardlinkRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void HardlinkRespMsg_init(HardlinkRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_HardlinkResp);
}

int HardlinkRespMsg_getValue(HardlinkRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}


#endif /*HARDLINKRESPMSG_H_*/
