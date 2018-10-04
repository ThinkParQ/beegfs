#ifndef REMOVENODEMSGEX_H_
#define REMOVENODEMSGEX_H_

#include <common/net/message/NetMessage.h>


struct RemoveNodeMsgEx;
typedef struct RemoveNodeMsgEx RemoveNodeMsgEx;

static inline void RemoveNodeMsgEx_init(RemoveNodeMsgEx* this);
static inline void RemoveNodeMsgEx_initFromNodeData(RemoveNodeMsgEx* this,
   NumNodeID nodeNumID, NodeType nodeType);

// virtual functions
extern void RemoveNodeMsgEx_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool RemoveNodeMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx);
extern bool __RemoveNodeMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);

// getters & setters
static inline NumNodeID RemoveNodeMsgEx_getNodeNumID(RemoveNodeMsgEx* this);
static inline NodeType RemoveNodeMsgEx_getNodeType(RemoveNodeMsgEx* this);
static inline const char* RemoveNodeMsgEx_getAckID(RemoveNodeMsgEx* this);


struct RemoveNodeMsgEx
{
   NetMessage netMessage;

   NumNodeID nodeNumID;
   int16_t nodeType;
   unsigned ackIDLen;
   const char* ackID;
};

extern const struct NetMessageOps RemoveNodeMsgEx_Ops;

void RemoveNodeMsgEx_init(RemoveNodeMsgEx* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_RemoveNode, &RemoveNodeMsgEx_Ops);
}

/**
 * @param nodeID just a reference, so do not free it as long as you use this object!
 */
void RemoveNodeMsgEx_initFromNodeData(RemoveNodeMsgEx* this,
   NumNodeID nodeNumID, NodeType nodeType)
{
   RemoveNodeMsgEx_init(this);

   this->nodeNumID = nodeNumID;

   this->nodeType = (int16_t)nodeType;

   this->ackID = "";
   this->ackIDLen = 0;
}

NumNodeID RemoveNodeMsgEx_getNodeNumID(RemoveNodeMsgEx* this)
{
   return this->nodeNumID;
}

NodeType RemoveNodeMsgEx_getNodeType(RemoveNodeMsgEx* this)
{
   return (NodeType)this->nodeType;
}

const char* RemoveNodeMsgEx_getAckID(RemoveNodeMsgEx* this)
{
   return this->ackID;
}

#endif /* REMOVENODEMSGEX_H_ */
