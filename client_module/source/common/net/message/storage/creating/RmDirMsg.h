#ifndef RMDIRMSG_H_
#define RMDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>

#define RMDIRMSG_FLAG_HAS_EVENT       1 /* contains file event logging information */

struct RmDirMsg;
typedef struct RmDirMsg RmDirMsg;

static inline void RmDirMsg_init(RmDirMsg* this);
static inline void RmDirMsg_initFromEntryInfo(RmDirMsg* this, const EntryInfo* parentInfo,
   const char* delDirName, const struct FileEvent* fileEvent);

// virtual functions
extern void RmDirMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

// inliners

struct RmDirMsg
{
   NetMessage netMessage;

   // for serialization
   const EntryInfo* parentInfo; // not owned by this object!
   const char* delDirName;      // not owned by this object!
   unsigned delDirNameLen;
   const struct FileEvent* fileEvent;

   // for deserialization
};

extern const struct NetMessageOps RmDirMsg_Ops;

void RmDirMsg_init(RmDirMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_RmDir, &RmDirMsg_Ops);
}

/**
 * @param entryInfo  just a reference, so do not free it as long as you use this object!
 * @param delDirName just a reference, so do not free it as long as you use this object!
 */
void RmDirMsg_initFromEntryInfo(RmDirMsg* this, const EntryInfo* parentInfo,
   const char* delDirName, const struct FileEvent* fileEvent)
{
   RmDirMsg_init(this);

   this->parentInfo = parentInfo;

   this->delDirName = delDirName;
   this->delDirNameLen = strlen(delDirName);
   this->fileEvent = fileEvent;

   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= RMDIRMSG_FLAG_HAS_EVENT;
}

#endif /*RMDIRMSG_H_*/
