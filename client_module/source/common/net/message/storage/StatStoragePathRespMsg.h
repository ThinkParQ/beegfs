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

// getters & setters
static inline int StatStoragePathRespMsg_getResult(StatStoragePathRespMsg* this);
static inline int64_t StatStoragePathRespMsg_getSizeTotal(StatStoragePathRespMsg* this);
static inline int64_t StatStoragePathRespMsg_getSizeFree(StatStoragePathRespMsg* this);
static inline int64_t StatStoragePathRespMsg_getInodesTotal(StatStoragePathRespMsg* this);
static inline int64_t StatStoragePathRespMsg_getInodesFree(StatStoragePathRespMsg* this);


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

int StatStoragePathRespMsg_getResult(StatStoragePathRespMsg* this)
{
   return this->result;
}

int64_t StatStoragePathRespMsg_getSizeTotal(StatStoragePathRespMsg* this)
{
   return this->sizeTotal;
}

int64_t StatStoragePathRespMsg_getSizeFree(StatStoragePathRespMsg* this)
{
   return this->sizeFree;
}

int64_t StatStoragePathRespMsg_getInodesTotal(StatStoragePathRespMsg* this)
{
   return this->inodesTotal;
}

int64_t StatStoragePathRespMsg_getInodesFree(StatStoragePathRespMsg* this)
{
   return this->inodesFree;
}

#endif /*STATSTORAGEPATHRESPMSG_H_*/
