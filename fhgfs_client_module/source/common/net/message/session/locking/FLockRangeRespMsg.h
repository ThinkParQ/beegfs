#ifndef FLOCKRANGERESPMSG_H_
#define FLOCKRANGERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>

/**
 * This message is for deserialiazation (incoming) only, serialization is not implemented!!
 */

struct FLockRangeRespMsg;
typedef struct FLockRangeRespMsg FLockRangeRespMsg;

static inline void FLockRangeRespMsg_init(FLockRangeRespMsg* this);

// getters & setters
static inline FhgfsOpsErr FLockRangeRespMsg_getResult(FLockRangeRespMsg* this);

struct FLockRangeRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void FLockRangeRespMsg_init(FLockRangeRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_FLockRangeResp);
}

FhgfsOpsErr FLockRangeRespMsg_getResult(FLockRangeRespMsg* this)
{
   return (FhgfsOpsErr)SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}



#endif /* FLOCKRANGERESPMSG_H_ */
