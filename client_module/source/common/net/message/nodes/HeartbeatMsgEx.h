#ifndef HEARTBEATMSG_H_
#define HEARTBEATMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/net/sock/NetworkInterfaceCard.h>


struct HeartbeatMsgEx;
typedef struct HeartbeatMsgEx HeartbeatMsgEx;

static inline void HeartbeatMsgEx_init(HeartbeatMsgEx* this);
static inline void HeartbeatMsgEx_initFromNodeData(HeartbeatMsgEx* this,
   const char* nodeID, NumNodeID nodeNumID, int nodeType, NicAddressList* nicList);

extern void __HeartbeatMsgEx_processIncomingRoot(HeartbeatMsgEx* this, struct App* app);

// virtual functions
extern void HeartbeatMsgEx_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool HeartbeatMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx);
extern bool __HeartbeatMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);

// inliners
static inline void HeartbeatMsgEx_parseNicList(HeartbeatMsgEx* this,
   NicAddressList* outNicList);

// getters & setters
static inline const char* HeartbeatMsgEx_getNodeID(HeartbeatMsgEx* this);
static inline NumNodeID HeartbeatMsgEx_getNodeNumID(HeartbeatMsgEx* this);
static inline int HeartbeatMsgEx_getNodeType(HeartbeatMsgEx* this);
static inline NumNodeID HeartbeatMsgEx_getRootNumID(HeartbeatMsgEx* this);
static inline const char* HeartbeatMsgEx_getAckID(HeartbeatMsgEx* this);
static inline void HeartbeatMsgEx_setPorts(HeartbeatMsgEx* this,
   uint16_t portUDP, uint16_t portTCP);
static inline uint16_t HeartbeatMsgEx_getPortUDP(HeartbeatMsgEx* this);
static inline uint16_t HeartbeatMsgEx_getPortTCP(HeartbeatMsgEx* this);


struct HeartbeatMsgEx
{
   NetMessage netMessage;

   unsigned nodeIDLen;
   const char* nodeID;
   int nodeType;
   NumNodeID nodeNumID;
   NumNodeID rootNumID; // 0 means unknown/undefined
   bool rootIsBuddyMirrored;
   uint64_t instanceVersion; // not used currently
   uint64_t nicListVersion; // not used currently
   uint16_t portUDP; // 0 means "undefined"
   uint16_t portTCP; // 0 means "undefined"
   unsigned ackIDLen;
   const char* ackID;

   // for serialization
   NicAddressList* nicList; // not owned by this object

   // for deserialization
   // NETMSG_NICLISTELEM_SIZE defines the element size
   // see NETMSG_NICLISTELEM_SIZE for element structure
   RawList rawNicList;
};

extern const struct NetMessageOps HeartbeatMsgEx_Ops;

void HeartbeatMsgEx_init(HeartbeatMsgEx* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_Heartbeat, &HeartbeatMsgEx_Ops);
}

/**
 * @param nodeID just a reference, so do not free it as long as you use this object
 * @param nicList just a reference, so do not free it as long as you use this object
 */
void HeartbeatMsgEx_initFromNodeData(HeartbeatMsgEx* this,
   const char* nodeID, NumNodeID nodeNumID, int nodeType, NicAddressList* nicList)
{
   HeartbeatMsgEx_init(this);

   this->nodeID = nodeID;
   this->nodeIDLen = strlen(nodeID);
   this->nodeNumID = nodeNumID;

   this->nodeType = nodeType;

   this->rootNumID = (NumNodeID){0}; // 0 means undefined/unknown
   this->rootIsBuddyMirrored = false;

   this->instanceVersion = 0; // reserverd for future use

   this->nicListVersion = 0; // reserverd for future use
   this->nicList = nicList;

   this->portUDP = 0; // 0 means "undefined"
   this->portTCP = 0; // 0 means "undefined"

   this->ackID = "";
   this->ackIDLen = 0;

}

void HeartbeatMsgEx_parseNicList(HeartbeatMsgEx* this, NicAddressList* outNicList)
{
   Serialization_deserializeNicList(&this->rawNicList, outNicList);
}

const char* HeartbeatMsgEx_getNodeID(HeartbeatMsgEx* this)
{
   return this->nodeID;
}

NumNodeID HeartbeatMsgEx_getNodeNumID(HeartbeatMsgEx* this)
{
   return this->nodeNumID;
}

int HeartbeatMsgEx_getNodeType(HeartbeatMsgEx* this)
{
   return this->nodeType;
}

NumNodeID HeartbeatMsgEx_getRootNumID(HeartbeatMsgEx* this)
{
   return this->rootNumID;
}

const char* HeartbeatMsgEx_getAckID(HeartbeatMsgEx* this)
{
   return this->ackID;
}

void HeartbeatMsgEx_setPorts(HeartbeatMsgEx* this, uint16_t portUDP, uint16_t portTCP)
{
   this->portUDP = portUDP;
   this->portTCP = portTCP;
}

uint16_t HeartbeatMsgEx_getPortUDP(HeartbeatMsgEx* this)
{
   return this->portUDP;
}

uint16_t HeartbeatMsgEx_getPortTCP(HeartbeatMsgEx* this)
{
   return this->portTCP;
}


#endif /*HEARTBEATMSG_H_*/
