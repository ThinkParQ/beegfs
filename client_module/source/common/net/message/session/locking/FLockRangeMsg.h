#ifndef FLOCKRANGEMSG_H_
#define FLOCKRANGEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

/**
 * This message is for serialiazation (outgoing) only, deserialization is not implemented!!
 */

struct FLockRangeMsg;
typedef struct FLockRangeMsg FLockRangeMsg;

static inline void FLockRangeMsg_init(FLockRangeMsg* this);
static inline void FLockRangeMsg_initFromSession(FLockRangeMsg* this, NumNodeID clientNumID,
   const char* fileHandleID, const EntryInfo* entryInfo,
   int ownerPID, int lockTypeFlags, uint64_t start, uint64_t end, const char* lockAckID);

// virtual functions
extern void FLockRangeMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct FLockRangeMsg
{
   NetMessage netMessage;

   NumNodeID clientNumID;
   const char* fileHandleID;
   unsigned fileHandleIDLen;
   const EntryInfo* entryInfoPtr; // not owned by this object
   int ownerPID; // pid on client (just informative, because shared on fork() )
   int lockTypeFlags; // ENTRYLOCKTYPE_...
   uint64_t start;
   uint64_t end;
   const char* lockAckID; // ID for ack message when log is granted
   unsigned lockAckIDLen;
};

extern const struct NetMessageOps FLockRangeMsg_Ops;

void FLockRangeMsg_init(FLockRangeMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_FLockRange, &FLockRangeMsg_Ops);
}

/**
 * @param fileHandleID just a reference, so do not free it as long as you use this object!
 * @param entryInfo just a reference, so do not free it as long as you use this object!
 */
void FLockRangeMsg_initFromSession(FLockRangeMsg* this, NumNodeID clientNumID,
   const char* fileHandleID, const EntryInfo* entryInfo,
   int ownerPID, int lockTypeFlags, uint64_t start, uint64_t end, const char* lockAckID)
{
   FLockRangeMsg_init(this);

   this->clientNumID = clientNumID;

   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);

   this->entryInfoPtr = entryInfo;

   this->ownerPID = ownerPID;
   this->lockTypeFlags = lockTypeFlags;

   this->start = start;
   this->end = end;

   this->lockAckID = lockAckID;
   this->lockAckIDLen = strlen(lockAckID);
}

#endif /* FLOCKRANGEMSG_H_ */
