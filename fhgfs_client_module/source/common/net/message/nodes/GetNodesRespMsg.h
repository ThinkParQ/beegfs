#ifndef GETNODESRESPMSG_H_
#define GETNODESRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NodeList.h>

/*
 * note: serialization not implemented
 */
struct GetNodesRespMsg;
typedef struct GetNodesRespMsg GetNodesRespMsg;

static inline void GetNodesRespMsg_init(GetNodesRespMsg* this);

// virtual functions
extern bool GetNodesRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// inliners
static inline void GetNodesRespMsg_parseNodeList(App* app, GetNodesRespMsg* this,
   NodeList* outNodeList);

// getters & setters
static inline NumNodeID GetNodesRespMsg_getRootNumID(GetNodesRespMsg* this);
static inline bool GetNodesRespMsg_getRootIsBuddyMirrored(GetNodesRespMsg* this);


struct GetNodesRespMsg
{
   NetMessage netMessage;

   NumNodeID rootNumID;
   bool rootIsBuddyMirrored;

   // for deserialization
   RawList rawNodeList;
};

extern const struct NetMessageOps GetNodesRespMsg_Ops;

void GetNodesRespMsg_init(GetNodesRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetNodesResp, &GetNodesRespMsg_Ops);

   this->rootNumID = (NumNodeID){0};
}

void GetNodesRespMsg_parseNodeList(App* app, GetNodesRespMsg* this, NodeList* outNodeList)
{
   Serialization_deserializeNodeList(app, &this->rawNodeList, outNodeList);
}

NumNodeID GetNodesRespMsg_getRootNumID(GetNodesRespMsg* this)
{
   return this->rootNumID;
}

bool GetNodesRespMsg_getRootIsBuddyMirrored(GetNodesRespMsg* this)
{
   return this->rootIsBuddyMirrored;
}

#endif /* GETNODESRESPMSG_H_ */
