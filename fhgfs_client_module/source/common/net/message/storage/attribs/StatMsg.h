#ifndef STATMSG_H_
#define STATMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/toolkit/MetadataTk.h>

#define STATMSG_FLAG_GET_PARENTINFO 1 /* caller wants to have parentOwnerNodeID and parentEntryID */


struct StatMsg;
typedef struct StatMsg StatMsg;

static inline void StatMsg_init(StatMsg* this);
static inline void StatMsg_initFromEntryInfo(StatMsg* this, const EntryInfo* entryInfo);
static inline void StatMsg_addParentInfoRequest(StatMsg* this);

// virtual functions
extern void StatMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern unsigned StatMsg_getSupportedHeaderFeatureFlagsMask(NetMessage* this);


struct StatMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfoPtr; // not owed by this object
};

extern const struct NetMessageOps StatMsg_Ops;

void StatMsg_init(StatMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_Stat, &StatMsg_Ops);
}

/**
 * @param entryInfo just a reference, so do not free it as long as you use this object!
 */
void StatMsg_initFromEntryInfo(StatMsg* this, const EntryInfo* entryInfo)
{
   StatMsg_init(this);

   this->entryInfoPtr = entryInfo;
}

void StatMsg_addParentInfoRequest(StatMsg* this)
{
   NetMessage* netMsg =  (NetMessage*) this;

   NetMessage_addMsgHeaderFeatureFlag(netMsg, STATMSG_FLAG_GET_PARENTINFO);
}


#endif /*STATMSG_H_*/
