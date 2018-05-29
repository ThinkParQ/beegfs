#ifndef LISTDIRFROMOFFSETRESPMSG_H_
#define LISTDIRFROMOFFSETRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>


struct ListDirFromOffsetRespMsg;
typedef struct ListDirFromOffsetRespMsg ListDirFromOffsetRespMsg;

static inline void ListDirFromOffsetRespMsg_init(ListDirFromOffsetRespMsg* this);

// virtual functions
extern bool ListDirFromOffsetRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// inliners
static inline void ListDirFromOffsetRespMsg_parseNames(ListDirFromOffsetRespMsg* this,
   StrCpyVec* outNames);
static inline void ListDirFromOffsetRespMsg_parseEntryIDs(ListDirFromOffsetRespMsg* this,
   StrCpyVec* outEntryIDs);
static inline void ListDirFromOffsetRespMsg_parseEntryTypes(ListDirFromOffsetRespMsg* this,
   UInt8Vec* outEntryTypes);
static inline void ListDirFromOffsetRespMsg_parseServerOffsets(ListDirFromOffsetRespMsg* this,
   Int64CpyVec* outServerOffsets);

// getters & setters
static inline int ListDirFromOffsetRespMsg_getResult(ListDirFromOffsetRespMsg* this);
static inline uint64_t ListDirFromOffsetRespMsg_getNewServerOffset(ListDirFromOffsetRespMsg* this);


/**
 * Note: Only deserialization supported, no serialization.
 */
struct ListDirFromOffsetRespMsg
{
   NetMessage netMessage;

   int result;

   int64_t newServerOffset;

   // for deserialization
   RawList namesList;
   RawList entryTypesList;
   RawList entryIDsList;
   RawList serverOffsetsList;
};

extern const struct NetMessageOps ListDirFromOffsetRespMsg_Ops;

void ListDirFromOffsetRespMsg_init(ListDirFromOffsetRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_ListDirFromOffsetResp,
      &ListDirFromOffsetRespMsg_Ops);
}

void ListDirFromOffsetRespMsg_parseEntryIDs(ListDirFromOffsetRespMsg* this, StrCpyVec* outEntryIDs)
{
   Serialization_deserializeStrCpyVec(&this->entryIDsList, outEntryIDs);
}

void ListDirFromOffsetRespMsg_parseNames(ListDirFromOffsetRespMsg* this, StrCpyVec* outNames)
{
   Serialization_deserializeStrCpyVec(&this->namesList, outNames);
}

void ListDirFromOffsetRespMsg_parseEntryTypes(ListDirFromOffsetRespMsg* this,
   UInt8Vec* outEntryTypes)
{
   Serialization_deserializeUInt8Vec(&this->entryTypesList, outEntryTypes);
}

void ListDirFromOffsetRespMsg_parseServerOffsets(ListDirFromOffsetRespMsg* this,
   Int64CpyVec* outServerOffsets)
{
   Serialization_deserializeInt64CpyVec(&this->serverOffsetsList, outServerOffsets);
}

int ListDirFromOffsetRespMsg_getResult(ListDirFromOffsetRespMsg* this)
{
   return this->result;
}

uint64_t ListDirFromOffsetRespMsg_getNewServerOffset(ListDirFromOffsetRespMsg* this)
{
   return this->newServerOffset;
}


#endif /*LISTDIRFROMOFFSETRESPMSG_H_*/
