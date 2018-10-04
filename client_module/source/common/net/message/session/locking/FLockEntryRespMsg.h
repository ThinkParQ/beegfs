#ifndef FLOCKENTRYRESPMSG_H_
#define FLOCKENTRYRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>

/**
 * This message is for deserialiazation (incoming) only, serialization is not implemented!!
 */

struct FLockEntryRespMsg;
typedef struct FLockEntryRespMsg FLockEntryRespMsg;

static inline void FLockEntryRespMsg_init(FLockEntryRespMsg* this);

struct FLockEntryRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void FLockEntryRespMsg_init(FLockEntryRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_FLockEntryResp);
}

#endif /* FLOCKENTRYRESPMSG_H_ */
