#ifndef SETATTRMSG_H_
#define SETATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/EntryInfo.h>


#define SETATTRMSG_FLAG_USE_QUOTA            1 /* if the message contains quota informations */
#define SETATTRMSG_FLAG_BUDDYMIRROR_SECOND   2 /* secondary of group, otherwise primary */


struct SetAttrMsg;
typedef struct SetAttrMsg SetAttrMsg;

static inline void SetAttrMsg_init(SetAttrMsg* this);
static inline void SetAttrMsg_initFromEntryInfo(SetAttrMsg* this, const EntryInfo* entryInfo,
   int validAttribs, SettableFileAttribs* attribs);

// virtual functions
extern void SetAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

// getters & setters
static inline int SetAttrMsg_getValidAttribs(SetAttrMsg* this);
static inline SettableFileAttribs* SetAttrMsg_getAttribs(SetAttrMsg* this);


struct SetAttrMsg
{
   NetMessage netMessage;

   int validAttribs;
   SettableFileAttribs attribs;

   // for serialization
   const EntryInfo* entryInfoPtr;
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
   SettableFileAttribs* attribs)
{
   SetAttrMsg_init(this);

   this->entryInfoPtr = entryInfo;
   this->validAttribs = validAttribs;
   this->attribs = *attribs;
}

int SetAttrMsg_getValidAttribs(SetAttrMsg* this)
{
   return this->validAttribs;
}

SettableFileAttribs* SetAttrMsg_getAttribs(SetAttrMsg* this)
{
   return &this->attribs;
}


#endif /*SETATTRMSG_H_*/
