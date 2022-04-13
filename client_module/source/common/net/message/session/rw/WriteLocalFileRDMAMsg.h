#ifndef WRITELOCALFILERDMAMSG_H_
#define WRITELOCALFILERDMAMSG_H_
#ifdef BEEGFS_NVFS

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/PathInfo.h>
#include <common/storage/RdmaInfo.h>
#include <app/log/Logger.h>
#include <app/App.h>
#include <os/iov_iter.h>


#define WRITELOCALFILERDMAMSG_FLAG_SESSION_CHECK        1 /* if session check should be done */
#define WRITELOCALFILERDMAMSG_FLAG_USE_QUOTA            2 /* if msg contains quota info */
#define WRITELOCALFILERDMAMSG_FLAG_DISABLE_IO           4 /* disable write syscall for net bench mode */
#define WRITELOCALFILERDMAMSG_FLAG_BUDDYMIRROR          8 /* given targetID is a buddymirrorgroup ID */
#define WRITELOCALFILERDMAMSG_FLAG_BUDDYMIRROR_SECOND  16 /* secondary of group, otherwise primary */
#define WRITELOCALFILERDMAMSG_FLAG_BUDDYMIRROR_FORWARD 32 /* forward msg to secondary */


struct WriteLocalFileRDMAMsg;
typedef struct WriteLocalFileRDMAMsg WriteLocalFileRDMAMsg;


static inline void WriteLocalFileRDMAMsg_init(WriteLocalFileRDMAMsg* this);
static inline void WriteLocalFileRDMAMsg_initFromSession(WriteLocalFileRDMAMsg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID, PathInfo* pathInfo,
   unsigned accessFlags, int64_t offset, int64_t count, RdmaInfo *rdmap);

// virtual functions
extern void WriteLocalFileRDMAMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

// getters & setters
static inline void WriteLocalFileRDMAMsg_setUserdataForQuota(WriteLocalFileRDMAMsg* this, unsigned userID,
   unsigned groupID);


/**
 * Note: This message supports only serialization, deserialization is not implemented.
 */
struct WriteLocalFileRDMAMsg
{
   NetMessage netMessage;

   int64_t offset;
   int64_t count;
   unsigned accessFlags;
   NumNodeID clientNumID;
   const char* fileHandleID;
   unsigned fileHandleIDLen;
   uint16_t targetID;
   PathInfo* pathInfo;
   unsigned userID;
   unsigned groupID;
   RdmaInfo *rdmap;
};

extern const struct NetMessageOps WriteLocalFileRDMAMsg_Ops;

void WriteLocalFileRDMAMsg_init(WriteLocalFileRDMAMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_WriteLocalFileRDMA, &WriteLocalFileRDMAMsg_Ops);
}

/**
 * @param fileHandleID just a reference, so do not free it as long as you use this object!
 * @param pathInfo just a reference, so do not free it as long as you use this object!
 */
void WriteLocalFileRDMAMsg_initFromSession(WriteLocalFileRDMAMsg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID, PathInfo* pathInfo,
   unsigned accessFlags, int64_t offset, int64_t count, RdmaInfo *rdmap)
{
   WriteLocalFileRDMAMsg_init(this);

   this->clientNumID = clientNumID;

   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);

   this->targetID = targetID;
   this->pathInfo = pathInfo;

   this->accessFlags = accessFlags;

   this->offset = offset;
   this->count = count;

   this->rdmap = rdmap;
}

void WriteLocalFileRDMAMsg_setUserdataForQuota(WriteLocalFileRDMAMsg* this, unsigned userID,
   unsigned groupID)
{
   NetMessage* thisCast = (NetMessage*) this;

   NetMessage_addMsgHeaderFeatureFlag(thisCast, WRITELOCALFILERDMAMSG_FLAG_USE_QUOTA);

   this->userID = userID;
   this->groupID = groupID;
}

#endif /* BEEGFS_NVFS */
#endif /*WRITELOCALFILERDMAMSG_H_*/
