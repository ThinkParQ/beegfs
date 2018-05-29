#ifndef FSYNCLOCALFILEMSG_H_
#define FSYNCLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>


#define FSYNCLOCALFILEMSG_FLAG_NO_SYNC          1 /* if a sync is not needed */
#define FSYNCLOCALFILEMSG_FLAG_SESSION_CHECK    2 /* if session check should be done */
#define FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR      4 /* given targetID is a buddymirrorgroup ID */
#define FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND 8 /* secondary of group, otherwise primary */


struct FSyncLocalFileMsg;
typedef struct FSyncLocalFileMsg FSyncLocalFileMsg;

static inline void FSyncLocalFileMsg_init(FSyncLocalFileMsg* this);
static inline void FSyncLocalFileMsg_initFromSession(FSyncLocalFileMsg* this,
   NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID);

// virtual functions
extern void FSyncLocalFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);



/**
 * Note: This message supports only serialization, deserialization is not implemented.
 */
struct FSyncLocalFileMsg
{
   NetMessage netMessage;

   NumNodeID clientNumID;
   const char* fileHandleID;
   unsigned fileHandleIDLen;
   uint16_t targetID;
};
extern const struct NetMessageOps FSyncLocalFileMsg_Ops;

void FSyncLocalFileMsg_init(FSyncLocalFileMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_FSyncLocalFile, &FSyncLocalFileMsg_Ops);
}

/**
 * @param fileHandleID just a reference, so do not free it as long as you use this object!
 */
void FSyncLocalFileMsg_initFromSession(FSyncLocalFileMsg* this, NumNodeID clientNumID,
   const char* fileHandleID, uint16_t targetID)
{
   FSyncLocalFileMsg_init(this);

   this->clientNumID = clientNumID;

   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);

   this->targetID = targetID;
}

#endif /*FSYNCLOCALFILEMSG_H_*/
