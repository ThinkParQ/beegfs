#ifndef SETXATTRMSG_H_
#define SETXATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

struct SetXAttrMsg;
typedef struct SetXAttrMsg SetXAttrMsg;

static inline void SetXAttrMsg_init(SetXAttrMsg* this);
static inline void SetXAttrMsg_initFromEntryInfoNameValueAndSize(SetXAttrMsg* this,
      const EntryInfo* entryInfo, const char* name, const char* value, const size_t size,
      const int flags);

// virtual functions
extern void SetXAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

struct SetXAttrMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfoPtr;

   const char* name;
   const char* value;
   size_t size;
   int flags;
};

extern const struct NetMessageOps SetXAttrMsg_Ops;

void SetXAttrMsg_init(SetXAttrMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_SetXAttr, &SetXAttrMsg_Ops);
}

void SetXAttrMsg_initFromEntryInfoNameValueAndSize(SetXAttrMsg* this, const EntryInfo* entryInfo,
      const char* name, const char* value, const size_t size, const int flags)
{
   SetXAttrMsg_init(this);
   this->entryInfoPtr = entryInfo;
   this->name = name;
   this->value = value;
   this->size = size;
   this->flags = flags;
}

#endif /*SETXATTRMSG_H_*/
