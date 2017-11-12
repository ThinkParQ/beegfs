#ifndef UNLINKFILEMSG_H_
#define UNLINKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>

#define UNLINKFILEMSG_FLAG_HAS_EVENT       1 /* contains file event logging information */

struct UnlinkFileMsg;
typedef struct UnlinkFileMsg UnlinkFileMsg;

static inline void UnlinkFileMsg_init(UnlinkFileMsg* this);
static inline void UnlinkFileMsg_initFromEntryInfo(UnlinkFileMsg* this,
   const EntryInfo* parentInfo, const char* delFileName, const struct FileEvent* fileEvent);

// virtual functions
extern void UnlinkFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct UnlinkFileMsg
{
   NetMessage netMessage;

   // for serialization
   const EntryInfo* parentInfoPtr; // not owned by this object!
   const char* delFileName;        // file name to be delete, not owned by this object
   unsigned delFileNameLen;

   const struct FileEvent* fileEvent;
};

extern const struct NetMessageOps UnlinkFileMsg_Ops;

void UnlinkFileMsg_init(UnlinkFileMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_UnlinkFile, &UnlinkFileMsg_Ops);
}

/**
 * @param path just a reference, so do not free it as long as you use this object!
 */
void UnlinkFileMsg_initFromEntryInfo(UnlinkFileMsg* this, const EntryInfo* parentInfo,
   const char* delFileName, const struct FileEvent* fileEvent)
{
   UnlinkFileMsg_init(this);

   this->parentInfoPtr  = parentInfo;
   this->delFileName    = delFileName;
   this->delFileNameLen = strlen(delFileName);
   this->fileEvent = fileEvent;

   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= UNLINKFILEMSG_FLAG_HAS_EVENT;
}

#endif /*UNLINKFILEMSG_H_*/
