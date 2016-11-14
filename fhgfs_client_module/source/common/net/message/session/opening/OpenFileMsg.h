#ifndef OPENFILEMSG_H_
#define OPENFILEMSG_H_

#include <common/storage/EntryInfo.h>
#include <common/net/message/NetMessage.h>


#define OPENFILEMSG_FLAG_USE_QUOTA          1 /* if the message contains quota informations */


struct OpenFileMsg;
typedef struct OpenFileMsg OpenFileMsg;

static inline void OpenFileMsg_init(OpenFileMsg* this);
static inline void OpenFileMsg_initFromSession(OpenFileMsg* this,
   NumNodeID clientNumID, const EntryInfo* entryInfo, unsigned accessFlags);

// virtual functions
extern void OpenFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct OpenFileMsg
{
   NetMessage netMessage;

   NumNodeID clientNumID;
   unsigned sessionIDLen;
   const EntryInfo* entryInfoPtr; // not owned by this object
   unsigned accessFlags;
};

extern const struct NetMessageOps OpenFileMsg_Ops;

void OpenFileMsg_init(OpenFileMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_OpenFile, &OpenFileMsg_Ops);
}

/**
 * @param sessionID just a reference, so do not free it as long as you use this object!
 * @param entryInfoPtr just a reference, so do not free it as long as you use this object!
 * @param accessFlags OPENFILE_ACCESS_... flags
 */
void OpenFileMsg_initFromSession(OpenFileMsg* this,
   NumNodeID clientNumID, const EntryInfo* entryInfo, unsigned accessFlags)
{
   OpenFileMsg_init(this);

   this->clientNumID = clientNumID;

   this->entryInfoPtr = entryInfo;

   this->accessFlags = accessFlags;
}

#endif /*OPENFILEMSG_H_*/
