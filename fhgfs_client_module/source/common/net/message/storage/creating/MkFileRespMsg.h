#ifndef MKFILERESPMSG_H_
#define MKFILERESPMSG_H_

#include <common/storage/EntryInfo.h>
#include <common/net/message/NetMessage.h>

struct MkFileRespMsg;
typedef struct MkFileRespMsg MkFileRespMsg;

static inline void MkFileRespMsg_init(MkFileRespMsg* this);

// virtual functions
extern bool MkFileRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters & setters
static inline int MkFileRespMsg_getResult(MkFileRespMsg* this);
static inline const EntryInfo* MkFileRespMsg_getEntryInfo(MkFileRespMsg* this);


struct MkFileRespMsg
{
   NetMessage netMessage;

   int result;
   EntryInfo entryInfo;
};

extern const struct NetMessageOps MkFileRespMsg_Ops;

void MkFileRespMsg_init(MkFileRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_MkFileResp, &MkFileRespMsg_Ops);
}

int MkFileRespMsg_getResult(MkFileRespMsg* this)
{
   return this->result;
}

const EntryInfo* MkFileRespMsg_getEntryInfo(MkFileRespMsg* this)
{
   return &this->entryInfo;
}

#endif /*MKFILERESPMSG_H_*/
