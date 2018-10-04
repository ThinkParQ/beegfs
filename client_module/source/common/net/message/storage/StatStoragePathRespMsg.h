#ifndef STATSTORAGEPATHRESPMSG_H_
#define STATSTORAGEPATHRESPMSG_H_

#include <common/net/message/NetMessage.h>

/**
 * Only supports deserialization. Serialization is not impl'ed.
 */

struct StatStoragePathRespMsg;
typedef struct StatStoragePathRespMsg StatStoragePathRespMsg;

static inline void StatStoragePathRespMsg_init(StatStoragePathRespMsg* this);

// virtual functions
extern bool StatStoragePathRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);


struct StatStoragePathRespMsg
{
   NetMessage netMessage;

   int result;
   int64_t sizeTotal;
   int64_t sizeFree;
   int64_t inodesTotal;
   int64_t inodesFree;
};

extern const struct NetMessageOps StatStoragePathRespMsg_Ops;

void StatStoragePathRespMsg_init(StatStoragePathRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_StatStoragePathResp, &StatStoragePathRespMsg_Ops);
}

#endif /*STATSTORAGEPATHRESPMSG_H_*/
