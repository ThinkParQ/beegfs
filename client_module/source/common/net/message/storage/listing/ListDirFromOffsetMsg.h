#ifndef LISTDIRFROMOFFSETMSG_H_
#define LISTDIRFROMOFFSETMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/EntryInfo.h>

/**
 * This message supports only serialization. (deserialization not implemented)
 */

struct ListDirFromOffsetMsg;
typedef struct ListDirFromOffsetMsg ListDirFromOffsetMsg;

static inline void ListDirFromOffsetMsg_init(ListDirFromOffsetMsg* this);
static inline void ListDirFromOffsetMsg_initFromEntryInfo(ListDirFromOffsetMsg* this,
   const EntryInfo* entryInfo, int64_t serverOffset, unsigned maxOutNames, bool filterDots);

// virtual functions
extern void ListDirFromOffsetMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);



struct ListDirFromOffsetMsg
{
   NetMessage netMessage;

   int64_t serverOffset;
   unsigned maxOutNames;
   bool filterDots;

   // for serialization
   const EntryInfo* entryInfoPtr; // not owned by this object!

   // for deserialization
   EntryInfo entryInfo;
};

extern const struct NetMessageOps ListDirFromOffsetMsg_Ops;

void ListDirFromOffsetMsg_init(ListDirFromOffsetMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_ListDirFromOffset, &ListDirFromOffsetMsg_Ops);
}

/**
 * @param entryInfo just a reference, so do not free it as long as you use this object!
 * @param filterDots true if you don't want "." and ".." in the result list.
 */
void ListDirFromOffsetMsg_initFromEntryInfo(ListDirFromOffsetMsg* this, const EntryInfo* entryInfo,
   int64_t serverOffset, unsigned maxOutNames, bool filterDots)
{
   ListDirFromOffsetMsg_init(this);

   this->entryInfoPtr = entryInfo;

   this->serverOffset = serverOffset;

   this->maxOutNames = maxOutNames;

   this->filterDots = filterDots;
}

#endif /*LISTDIRFROMOFFSETMSG_H_*/
