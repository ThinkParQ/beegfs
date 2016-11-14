#ifndef MKDIRRESPMSG_H_
#define MKDIRRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

struct MkDirRespMsg;
typedef struct MkDirRespMsg MkDirRespMsg;

static inline void MkDirRespMsg_init(MkDirRespMsg* this);

// virtual functions
extern bool MkDirRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters & setters
static inline int MkDirRespMsg_getResult(MkDirRespMsg* this);
static inline const EntryInfo* MkDirRespMsg_getEntryInfo(MkDirRespMsg* this);


struct MkDirRespMsg
{
   NetMessage netMessage;

   int result;

   // for deserialization
   EntryInfo entryInfo;
};

extern const struct NetMessageOps MkDirRespMsg_Ops;

void MkDirRespMsg_init(MkDirRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_MkDirResp, &MkDirRespMsg_Ops);
}

int MkDirRespMsg_getResult(MkDirRespMsg* this)
{
   return this->result;
}

const EntryInfo* MkDirRespMsg_getEntryInfo(MkDirRespMsg* this)
{
   return &this->entryInfo;
}

#endif /*MKDIRRESPMSG_H_*/
