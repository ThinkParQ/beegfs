#ifndef CLOSEFILEMSG_H_
#define CLOSEFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>


#define CLOSEFILEMSG_FLAG_EARLYRESPONSE      1 /* send response before chunk files close */
#define CLOSEFILEMSG_FLAG_CANCELAPPENDLOCKS  2 /* cancel append locks of this file handle */

/**
 * This message supports sending (serialization) only, i.e. no deserialization implemented!!
 */

struct CloseFileMsg;
typedef struct CloseFileMsg CloseFileMsg;

static inline void CloseFileMsg_init(CloseFileMsg* this);
static inline void CloseFileMsg_initFromSession(CloseFileMsg* this, NumNodeID clientNumID,
   const char* fileHandleID, const EntryInfo* entryInfo, int maxUsedNodeIndex);

// virtual functions
extern void CloseFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

// getters & setters
static inline NumNodeID CloseFileMsg_getClientNumID(CloseFileMsg* this);
static inline const char* CloseFileMsg_getFileHandleID(CloseFileMsg* this);
static inline int CloseFileMsg_getMaxUsedNodeIndex(CloseFileMsg* this);


struct CloseFileMsg
{
   NetMessage netMessage;

   NumNodeID clientNumID;

   unsigned fileHandleIDLen;
   const char* fileHandleID;
   int maxUsedNodeIndex;

   // for serialization
   const EntryInfo* entryInfoPtr; // not owned by this object
};

extern const struct NetMessageOps CloseFileMsg_Ops;

void CloseFileMsg_init(CloseFileMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_CloseFile, &CloseFileMsg_Ops);
}

/**
 * @param fileHandleID just a reference, so do not free it as long as you use this object!
 * @param entryInfo just a reference, so do not free it as long as you use this object!
 */
void CloseFileMsg_initFromSession(CloseFileMsg* this, NumNodeID clientNumID,
   const char* fileHandleID, const EntryInfo* entryInfo, int maxUsedNodeIndex)
{
   CloseFileMsg_init(this);
   
   this->clientNumID = clientNumID;
   
   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);
   
   this->entryInfoPtr = entryInfo;

   this->maxUsedNodeIndex = maxUsedNodeIndex;
}

NumNodeID CloseFileMsg_getClientNumID(CloseFileMsg* this)
{
   return this->clientNumID;
}

const char* CloseFileMsg_getFileHandleID(CloseFileMsg* this)
{
   return this->fileHandleID;
}

int CloseFileMsg_getMaxUsedNodeIndex(CloseFileMsg* this)
{
   return this->maxUsedNodeIndex;
}


#endif /*CLOSEFILEMSG_H_*/
