#ifndef SETATTRMSG_H_
#define SETATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>


#define SETATTRMSG_FLAG_USE_QUOTA            1 /* if the message contains quota informations */
#define SETATTRMSG_FLAG_BUDDYMIRROR_SECOND   2 /* secondary of group, otherwise primary */
#define SETATTRMSG_FLAG_HAS_EVENT            4 /* contains file event logging information */


struct SetAttrMsg;
typedef struct SetAttrMsg SetAttrMsg;

static inline void SetAttrMsg_init(SetAttrMsg* this);
static inline void SetAttrMsg_initFromEntryInfo(SetAttrMsg* this, const EntryInfo* entryInfo,
   int validAttribs, SettableFileAttribs* attribs, const struct FileEvent* fileEvent);

// virtual functions
extern void SetAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);


struct SetAttrMsg
{
   NetMessage netMessage;

   int validAttribs;
   SettableFileAttribs attribs;

   // for serialization
   const EntryInfo* entryInfoPtr;
   const struct FileEvent* fileEvent;
};

extern const struct NetMessageOps SetAttrMsg_Ops;

void SetAttrMsg_init(SetAttrMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_SetAttr, &SetAttrMsg_Ops);
}

/**
 * @param entryInfo just a reference, so do not free it as long as you use this object!
 * @param validAttribs a combination of SETATTR_CHANGE_...-Flags
 */
void SetAttrMsg_initFromEntryInfo(SetAttrMsg* this, const EntryInfo* entryInfo, int validAttribs,
   SettableFileAttribs* attribs, const struct FileEvent* fileEvent)
{
   SetAttrMsg_init(this);

   this->entryInfoPtr = entryInfo;
   this->validAttribs = validAttribs;
   this->attribs = *attribs;
   this->fileEvent = fileEvent;

   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= SETATTRMSG_FLAG_HAS_EVENT;
}

#endif /*SETATTRMSG_H_*/
