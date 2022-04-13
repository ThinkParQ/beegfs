#ifndef READLOCALFILERDMAMSG_H_
#define READLOCALFILERDMAMSG_H_
#ifdef BEEGFS_NVFS
#include <common/net/message/NetMessage.h>
#include <common/storage/PathInfo.h>
#include <common/storage/RdmaInfo.h>
#include "ReadLocalFileV2Msg.h"


struct ReadLocalFileRDMAMsg;
typedef struct ReadLocalFileRDMAMsg ReadLocalFileRDMAMsg;

static inline void ReadLocalFileRDMAMsg_init(ReadLocalFileRDMAMsg* this);
static inline void ReadLocalFileRDMAMsg_initFromSession(ReadLocalFileRDMAMsg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID, PathInfo* pathInfoPtr,
   unsigned accessFlags, int64_t offset, int64_t count, RdmaInfo *rdmap);

// virtual functions
extern void ReadLocalFileRDMAMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct ReadLocalFileRDMAMsg
{
   NetMessage netMessage;

   int64_t offset;
   int64_t count;
   unsigned accessFlags;
   NumNodeID clientNumID;
   const char* fileHandleID;
   unsigned fileHandleIDLen;
   PathInfo* pathInfoPtr;
   uint16_t targetID;
   RdmaInfo *rdmap;
};

extern const struct NetMessageOps ReadLocalFileRDMAMsg_Ops;

void ReadLocalFileRDMAMsg_init(ReadLocalFileRDMAMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_ReadLocalFileRDMA, &ReadLocalFileRDMAMsg_Ops);
}

/**
 * @param sessionID just a reference, so do not free it as long as you use this object!
 */
void ReadLocalFileRDMAMsg_initFromSession(ReadLocalFileRDMAMsg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID, PathInfo* pathInfoPtr,
   unsigned accessFlags, int64_t offset, int64_t count, RdmaInfo *rdmap)
{
   ReadLocalFileRDMAMsg_init(this);

   this->clientNumID = clientNumID;

   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);

   this->targetID = targetID;

   this->pathInfoPtr = pathInfoPtr;

   this->accessFlags = accessFlags;

   this->offset = offset;
   this->count = count;

   this->rdmap = rdmap;
}

#endif /* BEEGFS_NVFS */
#endif /*READLOCALFILERDMAMSG_H_*/
