#ifndef HARDLINKMSG_H_
#define HARDLINKMSG_H_

#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

#define HARDLINKMSG_FLAG_HAS_EVENT           2 /* contains file event logging information */

/* This is the HardlinkMsg class, deserialization is not implemented, as the client is not
 * supposed to deserialize the message. There is also only a basic constructor, if a full
 * constructor is needed, it can be easily added later on */

struct HardlinkMsg;
typedef struct HardlinkMsg HardlinkMsg;

static inline void HardlinkMsg_init(HardlinkMsg* this);
static inline void HardlinkMsg_initFromEntryInfo(HardlinkMsg* this, const EntryInfo* fromDirInfo,
   const char* fromName, unsigned fromLen, const EntryInfo* fromInfo, const EntryInfo* toDirInfo,
   const char* toName, unsigned toNameLen, const struct FileEvent* fileEvent);

// virtual functions
extern void HardlinkMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct HardlinkMsg
{
   NetMessage netMessage;

   // for serialization

   const char* fromName;    // not owned by this object!
   const EntryInfo* fromInfo;     // not owned by this object!

   unsigned fromNameLen;
   const EntryInfo* fromDirInfo; // not owned by this object!

   const char* toName;    // not owned by this object!
   unsigned toNameLen;
   const EntryInfo* toDirInfo;  // not owned by this object!

   const struct FileEvent* fileEvent;
};

extern const struct NetMessageOps HardlinkMsg_Ops;

void HardlinkMsg_init(HardlinkMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_Hardlink, &HardlinkMsg_Ops);
}

/**
 * @param fromName just a reference, so do not free it as long as you use this object!
 * @param fromDirInfo just a reference, so do not free it as long as you use this object!
 * @param toName just a reference, so do not free it as long as you use this object!
 * @param toDirInfo just a reference, so do not free it as long as you use this object!
 */
void HardlinkMsg_initFromEntryInfo(HardlinkMsg* this, const EntryInfo* fromDirInfo,
   const char* fromName, unsigned fromLen, const EntryInfo* fromInfo, const EntryInfo* toDirInfo,
   const char* toName, unsigned toNameLen, const struct FileEvent* fileEvent)
{
   HardlinkMsg_init(this);

   this->fromName     = fromName;
   this->fromInfo     = fromInfo;

   this->fromNameLen  = fromLen;
   this->fromDirInfo  = fromDirInfo;

   this->toName       = toName;
   this->toNameLen    = toNameLen;
   this->toDirInfo    = toDirInfo;
   this->fileEvent = fileEvent;

   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= HARDLINKMSG_FLAG_HAS_EVENT;
}

#endif /*HARDLINKMSG_H_*/
