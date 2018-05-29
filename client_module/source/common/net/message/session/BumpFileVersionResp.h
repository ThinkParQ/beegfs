#ifndef BUMPFILEVERSIONRESPMSG_H_
#define BUMPFILEVERSIONRESPMSG_H_

#include "../SimpleIntMsg.h"

typedef struct BumpFileVersionRespMsg BumpFileVersionRespMsg;

struct BumpFileVersionRespMsg
{
   SimpleIntMsg base;
};


static inline void BumpFileVersionRespMsg_init(struct BumpFileVersionRespMsg* this)
{
   SimpleIntMsg_init(&this->base, NETMSGTYPE_BumpFileVersionResp);
}

#endif
