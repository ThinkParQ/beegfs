#ifndef GETFILEVERSIONRESPMSG_H_
#define GETFILEVERSIONRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageErrors.h>

typedef struct GetFileVersionRespMsg GetFileVersionRespMsg;

struct GetFileVersionRespMsg
{
   NetMessage base;

   FhgfsOpsErr result;
   uint64_t version;
};
extern const struct NetMessageOps GetFileVersionRespMsg_Ops;


static inline void GetFileVersionRespMsg_init(struct GetFileVersionRespMsg* this)
{
   NetMessage_init(&this->base, NETMSGTYPE_GetFileVersionResp, &GetFileVersionRespMsg_Ops);
}

#endif
