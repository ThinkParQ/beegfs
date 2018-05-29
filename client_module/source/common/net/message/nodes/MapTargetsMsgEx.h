#ifndef MAPTARGETSMSGEX_H_
#define MAPTARGETSMSGEX_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>
#include <common/storage/StoragePoolId.h>

/**
 * Note: Only the receive/deserialize part of this message is implemented (so it cannot be sent).
 * Note: Processing only sends response when ackID is set (no MapTargetsRespMsg implemented).
 */

struct MapTargetsMsgEx;
typedef struct MapTargetsMsgEx MapTargetsMsgEx;

static inline void MapTargetsMsgEx_init(MapTargetsMsgEx* this);

// virtual functions
extern bool MapTargetsMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx);
extern bool __MapTargetsMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);

// inliners
static inline void MapTargetsMsgEx_parseTargetIDs(MapTargetsMsgEx* this,
   UInt16List* outTargetIDs);

// getters & setters
static inline NumNodeID MapTargetsMsgEx_getNodeID(MapTargetsMsgEx* this);
static inline const char* MapTargetsMsgEx_getAckID(MapTargetsMsgEx* this);


struct MapTargetsMsgEx
{
   NetMessage netMessage;

   NumNodeID nodeID;
   unsigned ackIDLen;
   const char* ackID;

   // for deserialization
   RawList rawTargetsList;
};

extern const struct NetMessageOps MapTargetsMsgEx_Ops;

void MapTargetsMsgEx_init(MapTargetsMsgEx* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_MapTargets, &MapTargetsMsgEx_Ops);
}

/*
 * note: actually the servers send vectors, but this doesn't make a difference for deserialization
 * here
 */
void MapTargetsMsgEx_parseTargetIDs(MapTargetsMsgEx* this, UInt16List* outTargetIDs)
{
   DeserializeCtx ctx = { this->rawTargetsList.data, this->rawTargetsList.length };
   unsigned i;

   // read each list element
   for(i=0; i < this->rawTargetsList.elemCount; i++)
   {
      uint16_t targetId = 0;
      StoragePoolId storagePoolId = { 0 };

      Serialization_deserializeUShort(&ctx, &targetId);

      StoragePoolId_deserialize(&ctx, &storagePoolId);

      // note: we are not interested in the storage pool id, so we drop it
      UInt16List_append(outTargetIDs, targetId);
   }
}

NumNodeID MapTargetsMsgEx_getNodeID(MapTargetsMsgEx* this)
{
   return this->nodeID;
}

const char* MapTargetsMsgEx_getAckID(MapTargetsMsgEx* this)
{
   return this->ackID;
}



#endif /* MAPTARGETSMSGEX_H_ */
