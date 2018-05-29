#ifndef FLOCKAPPENDMSG_H_
#define FLOCKAPPENDMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

/**
 * This message is for serialiazation (outgoing) only, deserialization is not implemented!!
 */

struct FLockAppendMsg;
typedef struct FLockAppendMsg FLockAppendMsg;

static inline void FLockAppendMsg_init(FLockAppendMsg* this);
static inline void FLockAppendMsg_initFromSession(FLockAppendMsg* this, NumNodeID clientNumID,
   const char* fileHandleID, const EntryInfo* entryInfo, int64_t clientFD, int ownerPID,
   int lockTypeFlags, const char* lockAckID);

// virtual functions
extern void FLockAppendMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct FLockAppendMsg
{
   NetMessage netMessage;

   NumNodeID clientNumID;
   const char* fileHandleID;
   unsigned fileHandleIDLen;
   const EntryInfo* entryInfoPtr; // not owned by this object
   int64_t clientFD; /* client-wide unique identifier (typically not really the fd number)
                      * corresponds to 'struct file_lock* fileLock->fl_file on the client side'
                      */

   int ownerPID; // pid on client (just informative, because shared on fork() )
   int lockTypeFlags; // ENTRYLOCKTYPE_...
   const char* lockAckID; // ID for ack message when log is granted
   unsigned lockAckIDLen;
};

extern const struct NetMessageOps FLockAppendMsg_Ops;

void FLockAppendMsg_init(FLockAppendMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_FLockAppend, &FLockAppendMsg_Ops);
}

/**
 * @param fileHandleID just a reference, so do not free it as long as you use this object!
 * @param entryInfo just a reference, so do not free it as long as you use this object!
 */
void FLockAppendMsg_initFromSession(FLockAppendMsg* this, NumNodeID clientNumID,
   const char* fileHandleID, const EntryInfo* entryInfo, int64_t clientFD, int ownerPID,
   int lockTypeFlags, const char* lockAckID)
{
   FLockAppendMsg_init(this);

   this->clientNumID = clientNumID;

   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);

   this->entryInfoPtr = entryInfo;

   this->clientFD = clientFD;
   this->ownerPID = ownerPID;
   this->lockTypeFlags = lockTypeFlags;

   this->lockAckID = lockAckID;
   this->lockAckIDLen = strlen(lockAckID);
}

#endif /* FLOCKAPPENDMSG_H_ */
