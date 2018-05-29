#ifndef RENAMEMSG_H_
#define RENAMEMSG_H_

#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

#define RENAMEMSG_FLAG_HAS_EVENT       1 /* contains file event logging information */

/* This is the RenameMsg class, deserialization is not implemented, as the client is not supposed to
   deserialize the message. There is also only a basic constructor, if a full constructor is
   needed, it can be easily added later on */

struct RenameMsg;
typedef struct RenameMsg RenameMsg;

static inline void RenameMsg_init(RenameMsg* this);
static inline void RenameMsg_initFromEntryInfo(RenameMsg* this, const char* oldName,
   unsigned oldLen, DirEntryType entryType, const EntryInfo* fromDirInfo, const char* newName,
   unsigned newLen, const EntryInfo* toDirInfo, const struct FileEvent* fileEvent);

// virtual functions
extern void RenameMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct RenameMsg
{
   NetMessage netMessage;

   // for serialization

   const char* oldName;    // not owned by this object!
   unsigned oldNameLen;
   DirEntryType entryType;
   const EntryInfo* fromDirInfo; // not owned by this object!

   const char* newName;    // not owned by this object!
   unsigned newNameLen;
   const EntryInfo* toDirInfo;  // not owned by this object!

   const struct FileEvent* fileEvent;
};

extern const struct NetMessageOps RenameMsg_Ops;

void RenameMsg_init(RenameMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_Rename, &RenameMsg_Ops);
}

/**
 * @param oldName just a reference, so do not free it as long as you use this object!
 * @param fromDirInfo just a reference, so do not free it as long as you use this object!
 * @param newName just a reference, so do not free it as long as you use this object!
 * @param toDirInfo just a reference, so do not free it as long as you use this object!
 */
void RenameMsg_initFromEntryInfo(RenameMsg* this, const char* oldName, unsigned oldLen,
   DirEntryType entryType, const EntryInfo* fromDirInfo, const char* newName, unsigned newLen,
   const EntryInfo* toDirInfo, const struct FileEvent* fileEvent)
{
   RenameMsg_init(this);

   this->oldName     = oldName;
   this->oldNameLen  = oldLen;
   this->entryType   = entryType;
   this->fromDirInfo = fromDirInfo;

   this->newName     = newName;
   this->newNameLen  = newLen;
   this->toDirInfo   = toDirInfo;
   this->fileEvent = fileEvent;

   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= RENAMEMSG_FLAG_HAS_EVENT;
}

#endif /*RENAMEMSG_H_*/
