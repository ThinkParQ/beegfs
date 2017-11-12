#ifndef TRUNCFILEMSG_H_
#define TRUNCFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>


#define TRUNCFILEMSG_FLAG_USE_QUOTA       1 /* if the message contains quota informations */
#define TRUNCFILEMSG_FLAG_HAS_EVENT       2 /* contains file event logging information */


struct TruncFileMsg;
typedef struct TruncFileMsg TruncFileMsg;

static inline void TruncFileMsg_init(TruncFileMsg* this);
static inline void TruncFileMsg_initFromEntryInfo(TruncFileMsg* this, int64_t filesize,
   const EntryInfo* entryInfo, const struct FileEvent* fileEvent);

// virtual functions
extern void TruncFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);



struct TruncFileMsg
{
   NetMessage netMessage;

   int64_t filesize;

   // for serialization
   const EntryInfo* entryInfoPtr;  // not owned by this object!
   const struct FileEvent* fileEvent;
};

extern const struct NetMessageOps TruncFileMsg_Ops;

void TruncFileMsg_init(TruncFileMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_TruncFile, &TruncFileMsg_Ops);
}

/**
 * @param entryInfo just a reference, so do not free it as long as you use this object!
 */
void TruncFileMsg_initFromEntryInfo(TruncFileMsg* this, int64_t filesize,
   const EntryInfo* entryInfo, const struct FileEvent* fileEvent)
{
   TruncFileMsg_init(this);

   this->filesize = filesize;

   this->entryInfoPtr = entryInfo;
   this->fileEvent = fileEvent;

   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= TRUNCFILEMSG_FLAG_HAS_EVENT;
}

#endif /*TRUNCFILEMSG_H_*/
