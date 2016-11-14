#ifndef GETTARGETMAPPINGSRESPMSG_H_
#define GETTARGETMAPPINGSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

/**
 * Note: This message can only be received/deserialized (send/serialization not implemented)
 */


struct GetTargetMappingsRespMsg;
typedef struct GetTargetMappingsRespMsg GetTargetMappingsRespMsg;

static inline void GetTargetMappingsRespMsg_init(GetTargetMappingsRespMsg* this);

// virtual functions
extern bool GetTargetMappingsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// inliners
static inline void GetTargetMappingsRespMsg_parseTargetIDs(GetTargetMappingsRespMsg* this,
   UInt16List* outTargetIDs);
static inline void GetTargetMappingsRespMsg_parseNodeIDs(GetTargetMappingsRespMsg* this,
   NumNodeIDList* outNodeIDs);


struct GetTargetMappingsRespMsg
{
   NetMessage netMessage;

   // for serialization
   UInt16List* targetIDs; // not owned by this object!
   NumNodeIDList* nodeIDs; // not owned by this object!

   // for deserialization
   RawList rawTargetIDsList;
   RawList rawNodeIDsList;
};

extern const struct NetMessageOps GetTargetMappingsRespMsg_Ops;

void GetTargetMappingsRespMsg_init(GetTargetMappingsRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetTargetMappingsResp,
      &GetTargetMappingsRespMsg_Ops);
}

void GetTargetMappingsRespMsg_parseTargetIDs(GetTargetMappingsRespMsg* this,
   UInt16List* outTargetIDs)
{
   Serialization_deserializeUInt16List(&this->rawTargetIDsList, outTargetIDs);
}

void GetTargetMappingsRespMsg_parseNodeIDs(GetTargetMappingsRespMsg* this,
   NumNodeIDList* outNodeIDs)
{
   Serialization_deserializeNumNodeIDList(&this->rawNodeIDsList, outNodeIDs);
}


#endif /* GETTARGETMAPPINGSRESPMSG_H_ */
