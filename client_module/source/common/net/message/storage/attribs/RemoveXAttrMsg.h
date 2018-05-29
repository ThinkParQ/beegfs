#ifndef REMOVEXATTRMSG_H_
#define REMOVEXATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

struct RemoveXAttrMsg;
typedef struct RemoveXAttrMsg RemoveXAttrMsg;

static inline void RemoveXAttrMsg_init(RemoveXAttrMsg* this);
static inline void RemoveXAttrMsg_initFromEntryInfoAndName(RemoveXAttrMsg* this,
      const EntryInfo* entryInfo, const char* name);

// virtual functions
extern void RemoveXAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

struct RemoveXAttrMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfoPtr;
   const char* name;
};

extern const struct NetMessageOps RemoveXAttrMsg_Ops;

void RemoveXAttrMsg_init(RemoveXAttrMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_RemoveXAttr, &RemoveXAttrMsg_Ops);
}

void RemoveXAttrMsg_initFromEntryInfoAndName(RemoveXAttrMsg* this, const EntryInfo* entryInfo,
      const char* name)
{
   RemoveXAttrMsg_init(this);
   this->entryInfoPtr = entryInfo;
   this->name = name;
}

#endif /*REMOVEXATTRMSG_H_*/
