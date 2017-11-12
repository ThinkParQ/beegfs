#ifndef READLOCALFILEV2MSG_H_
#define READLOCALFILEV2MSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/PathInfo.h>


#define READLOCALFILEMSG_FLAG_SESSION_CHECK       1 /* if session check infos should be done */
#define READLOCALFILEMSG_FLAG_DISABLE_IO          2 /* disable read syscall for net bench */
#define READLOCALFILEMSG_FLAG_BUDDYMIRROR         4 /* given targetID is a buddymirrorgroup ID */
#define READLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND  8 /* secondary of group, otherwise primary */


struct ReadLocalFileV2Msg;
typedef struct ReadLocalFileV2Msg ReadLocalFileV2Msg;

static inline void ReadLocalFileV2Msg_init(ReadLocalFileV2Msg* this);
static inline void ReadLocalFileV2Msg_initFromSession(ReadLocalFileV2Msg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID, PathInfo* pathInfoPtr,
   unsigned accessFlags, int64_t offset, int64_t count);

// virtual functions
extern void ReadLocalFileV2Msg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct ReadLocalFileV2Msg
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
};

extern const struct NetMessageOps ReadLocalFileV2Msg_Ops;

void ReadLocalFileV2Msg_init(ReadLocalFileV2Msg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_ReadLocalFileV2, &ReadLocalFileV2Msg_Ops);
}

/**
 * @param sessionID just a reference, so do not free it as long as you use this object!
 */
void ReadLocalFileV2Msg_initFromSession(ReadLocalFileV2Msg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID, PathInfo* pathInfoPtr,
   unsigned accessFlags, int64_t offset, int64_t count)
{
   ReadLocalFileV2Msg_init(this);

   this->clientNumID = clientNumID;

   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);

   this->targetID = targetID;

   this->pathInfoPtr = pathInfoPtr;

   this->accessFlags = accessFlags;

   this->offset = offset;
   this->count = count;
}

#endif /*READLOCALFILEV2MSG_H_*/
