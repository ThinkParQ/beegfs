#ifndef REFRESHENTRYINFO_H_
#define REFRESHENTRYINFO_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>


struct RefreshEntryInfoMsg;
typedef struct RefreshEntryInfoMsg RefreshEntryInfoMsg;

static inline void RefreshEntryInfoMsg_init(RefreshEntryInfoMsg* this);
static inline void RefreshEntryInfoMsg_initFromEntryInfo(RefreshEntryInfoMsg* this, const EntryInfo* entryInfo);

// virtual functions
extern void RefreshEntryInfoMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct RefreshEntryInfoMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfoPtr; // not owned by this object
};

extern const struct NetMessageOps RefreshEntryInfoMsg_Ops;

void RefreshEntryInfoMsg_init(RefreshEntryInfoMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_RefreshEntryInfo, &RefreshEntryInfoMsg_Ops);
}

/**
 * @param entryInfo just a reference, so do not free it as long as you use this object!
 */
void RefreshEntryInfoMsg_initFromEntryInfo(RefreshEntryInfoMsg* this, const EntryInfo* entryInfo)
{
   RefreshEntryInfoMsg_init(this);

   this->entryInfoPtr = entryInfo;
}

#endif /*REFRESHENTRYINFO_H_*/
