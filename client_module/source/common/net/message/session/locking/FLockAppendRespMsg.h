#ifndef FLOCKAPPENDRESPMSG_H_
#define FLOCKAPPENDRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>

/**
 * This message is for deserialiazation (incoming) only, serialization is not implemented!!
 */

struct FLockAppendRespMsg;
typedef struct FLockAppendRespMsg FLockAppendRespMsg;

static inline void FLockAppendRespMsg_init(FLockAppendRespMsg* this);

struct FLockAppendRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void FLockAppendRespMsg_init(FLockAppendRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_FLockAppendResp);
}

#endif /* FLOCKAPPENDRESPMSG_H_ */
