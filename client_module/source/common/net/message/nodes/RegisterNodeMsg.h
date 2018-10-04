#ifndef REGISTERNODEMSG_H
#define REGISTERNODEMSG_H

#include <common/net/message/NetMessage.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <toolkit/BitStore.h>

struct RegisterNodeMsg;
typedef struct RegisterNodeMsg RegisterNodeMsg;

static inline void RegisterNodeMsg_init(RegisterNodeMsg* this);
static inline void RegisterNodeMsg_initFromNodeData(RegisterNodeMsg* this, const char* nodeID,
   const NumNodeID nodeNumID, const int nodeType, NicAddressList* nicList, const uint16_t portUDP);

// virtual functions
extern void RegisterNodeMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

struct RegisterNodeMsg
{
   NetMessage netMessage;

   const char* nodeID; // not owned by this object
   unsigned nodeIDLen;
   NumNodeID nodeNumID;

   NumNodeID rootNumID;
   bool rootIsBuddyMirrored;

   int nodeType;

   NicAddressList* nicList; // not owned by this object

   uint16_t portUDP;
   uint16_t portTCP;

   uint64_t instanceVersion; // not used currently
   uint64_t nicListVersion; // not used currently
};

extern const struct NetMessageOps RegisterNodeMsg_Ops;

void RegisterNodeMsg_init(RegisterNodeMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_RegisterNode, &RegisterNodeMsg_Ops);
}

/**
 * @param nodeID just a reference, so do not free it as long as you use this object
 * @param nicList just a reference, so do not free it as long as you use this object
 */
void RegisterNodeMsg_initFromNodeData(RegisterNodeMsg* this, const char* nodeID,
   const NumNodeID nodeNumID, const int nodeType,
   NicAddressList* nicList, const uint16_t portUDP)
{
   RegisterNodeMsg_init(this);

   this->nodeID = nodeID;
   this->nodeIDLen = strlen(nodeID);
   this->nodeNumID = nodeNumID;
   this->nodeType = nodeType;
   this->nicList = nicList;
   this->portUDP = portUDP;

   // not used in client, but general RegisterNodeMsg has it and expects it to be serialized
   this->rootNumID = (NumNodeID){0};
   this->rootIsBuddyMirrored = false;

   this->nicListVersion = 0; // undefined
   this->instanceVersion = 0; // undefined
   this->portTCP = 0; // undefined
}

#endif /*REGISTERNODEMSG_H*/
