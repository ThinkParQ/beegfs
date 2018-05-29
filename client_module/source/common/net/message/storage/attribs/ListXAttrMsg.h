#ifndef LISTXATTRMSG_H_
#define LISTXATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

struct ListXAttrMsg;
typedef struct ListXAttrMsg ListXAttrMsg;

static inline void ListXAttrMsg_init(ListXAttrMsg* this);
static inline void ListXAttrMsg_initFromEntryInfoAndSize(ListXAttrMsg* this,
      const EntryInfo* entryInfo, ssize_t size);

// virtual functions
extern void ListXAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct ListXAttrMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfoPtr; // not owned by this object
   int size; // buffer size for the list buffer of the listxattr call
};

extern const struct NetMessageOps ListXAttrMsg_Ops;

void ListXAttrMsg_init(ListXAttrMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_ListXAttr, &ListXAttrMsg_Ops);
}

/**
 * @param entryID just a reference, so do not free it as long as you use this object!
 */
void ListXAttrMsg_initFromEntryInfoAndSize(ListXAttrMsg* this, const EntryInfo* entryInfo,
      ssize_t size)
{
   ListXAttrMsg_init(this);
   this->entryInfoPtr = entryInfo;
   this->size = size;
}

#endif /*LISTXATTRMSG_H_*/
