#ifndef GETXATTRMSG_H_
#define GETXATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

struct GetXAttrMsg;
typedef struct GetXAttrMsg GetXAttrMsg;

static inline void GetXAttrMsg_init(GetXAttrMsg* this);
static inline void GetXAttrMsg_initFromEntryInfoNameAndSize(GetXAttrMsg* this,
      const EntryInfo* entryInfo, const char* name, ssize_t size);

// virtual functions
extern void GetXAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct GetXAttrMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfoPtr;

   const char* name;
   int size;
};

extern const struct NetMessageOps GetXAttrMsg_Ops;

void GetXAttrMsg_init(GetXAttrMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetXAttr, &GetXAttrMsg_Ops);
}

void GetXAttrMsg_initFromEntryInfoNameAndSize(GetXAttrMsg* this, const EntryInfo* entryInfo,
      const char* name, ssize_t size)
{
   GetXAttrMsg_init(this);
   this->entryInfoPtr = entryInfo;
   this->name = name;
   this->size = size;
}

#endif /*GETXATTRMSG_H_*/
