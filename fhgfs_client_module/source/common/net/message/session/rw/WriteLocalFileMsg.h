#ifndef WRITELOCALFILEMSG_H_
#define WRITELOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/PathInfo.h>
#include <app/log/Logger.h>
#include <app/App.h>
#include <os/iov_iter.h>


#define WRITELOCALFILEMSG_FLAG_SESSION_CHECK        1 /* if session check should be done */
#define WRITELOCALFILEMSG_FLAG_USE_QUOTA            2 /* if msg contains quota info */
#define WRITELOCALFILEMSG_FLAG_DISABLE_IO           4 /* disable write syscall for net bench mode */
#define WRITELOCALFILEMSG_FLAG_BUDDYMIRROR          8 /* given targetID is a buddymirrorgroup ID */
#define WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND  16 /* secondary of group, otherwise primary */
#define WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_FORWARD 32 /* forward msg to secondary */


struct WriteLocalFileMsg;
typedef struct WriteLocalFileMsg WriteLocalFileMsg;


static inline void WriteLocalFileMsg_init(WriteLocalFileMsg* this);
static inline void WriteLocalFileMsg_initFromSession(WriteLocalFileMsg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID, PathInfo* pathInfo,
   unsigned accessFlags, int64_t offset, int64_t count);

// virtual functions
extern void WriteLocalFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

// getters & setters
static inline void WriteLocalFileMsg_setUserdataForQuota(WriteLocalFileMsg* this, unsigned userID,
   unsigned groupID);


/**
 * Note: This message supports only serialization, deserialization is not implemented.
 */
struct WriteLocalFileMsg
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
};

extern const struct NetMessageOps WriteLocalFileMsg_Ops;

void WriteLocalFileMsg_init(WriteLocalFileMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_WriteLocalFile, &WriteLocalFileMsg_Ops);
}

/**
 * @param fileHandleID just a reference, so do not free it as long as you use this object!
 * @param pathInfo just a reference, so do not free it as long as you use this object!
 */
void WriteLocalFileMsg_initFromSession(WriteLocalFileMsg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID, PathInfo* pathInfo,
   unsigned accessFlags,
   int64_t offset, int64_t count)
{
   WriteLocalFileMsg_init(this);

   this->clientNumID = clientNumID;

   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);

   this->targetID = targetID;
   this->pathInfo = pathInfo;

   this->accessFlags = accessFlags;

   this->offset = offset;
   this->count = count;
}

void WriteLocalFileMsg_setUserdataForQuota(WriteLocalFileMsg* this, unsigned userID,
   unsigned groupID)
{
   NetMessage* thisCast = (NetMessage*) this;

   NetMessage_addMsgHeaderFeatureFlag(thisCast, WRITELOCALFILEMSG_FLAG_USE_QUOTA);

   this->userID = userID;
   this->groupID = groupID;
}

#endif /*WRITELOCALFILEMSG_H_*/
