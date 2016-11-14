#ifndef UNLINKFILEMSG_H_
#define UNLINKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>


struct UnlinkFileMsg;
typedef struct UnlinkFileMsg UnlinkFileMsg;

static inline void UnlinkFileMsg_init(UnlinkFileMsg* this);
static inline void UnlinkFileMsg_initFromEntryInfo(UnlinkFileMsg* this,
   const EntryInfo* parentInfo, const char* delFileName);

// virtual functions
extern void UnlinkFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct UnlinkFileMsg
{
   NetMessage netMessage;
   
   // for serialization
   const EntryInfo* parentInfoPtr; // not owned by this object!
   const char* delFileName;        // file name to be delete, not owned by this object
   unsigned delFileNameLen;

};

extern const struct NetMessageOps UnlinkFileMsg_Ops;

void UnlinkFileMsg_init(UnlinkFileMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_UnlinkFile, &UnlinkFileMsg_Ops);
}

/**
 * @param path just a reference, so do not free it as long as you use this object!
 */
void UnlinkFileMsg_initFromEntryInfo(UnlinkFileMsg* this, const EntryInfo* parentInfo,
   const char* delFileName)
{
   UnlinkFileMsg_init(this);
   
   this->parentInfoPtr  = parentInfo;
   this->delFileName    = delFileName;
   this->delFileNameLen = strlen(delFileName);
}

#endif /*UNLINKFILEMSG_H_*/
