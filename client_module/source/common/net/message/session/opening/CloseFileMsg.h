#ifndef CLOSEFILEMSG_H_
#define CLOSEFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>


#define CLOSEFILEMSG_FLAG_EARLYRESPONSE      1 /* send response before chunk files close */
#define CLOSEFILEMSG_FLAG_CANCELAPPENDLOCKS  2 /* cancel append locks of this file handle */
#define CLOSEFILEMSG_FLAG_HAS_EVENT          8 /* contains file event logging information */

/**
 * This message supports sending (serialization) only, i.e. no deserialization implemented!!
 */

struct CloseFileMsg;
typedef struct CloseFileMsg CloseFileMsg;

static inline void CloseFileMsg_init(CloseFileMsg* this);
static inline void CloseFileMsg_initFromSession(CloseFileMsg* this, NumNodeID clientNumID,
   const char* fileHandleID, const EntryInfo* entryInfo, int maxUsedNodeIndex,
   const struct FileEvent* fileEvent);

// virtual functions
extern void CloseFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct CloseFileMsg
{
   NetMessage netMessage;

   NumNodeID clientNumID;

   unsigned fileHandleIDLen;
   const char* fileHandleID;
   int maxUsedNodeIndex;

   // for serialization
   const EntryInfo* entryInfoPtr; // not owned by this object
   const struct FileEvent* fileEvent;
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
   const char* fileHandleID, const EntryInfo* entryInfo, int maxUsedNodeIndex,
   const struct FileEvent* fileEvent)
{
   CloseFileMsg_init(this);

   this->clientNumID = clientNumID;

   this->fileHandleID = fileHandleID;
   this->fileHandleIDLen = strlen(fileHandleID);

   this->entryInfoPtr = entryInfo;

   this->maxUsedNodeIndex = maxUsedNodeIndex;
   this->fileEvent = fileEvent;

   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= CLOSEFILEMSG_FLAG_HAS_EVENT;
}

#endif /*CLOSEFILEMSG_H_*/
