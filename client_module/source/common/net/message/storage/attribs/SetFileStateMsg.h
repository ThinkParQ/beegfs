#ifndef SETFILESTATEMSG_H_
#define SETFILESTATEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

struct SetFileStateMsg;
typedef struct SetFileStateMsg SetFileStateMsg;

static inline void SetFileStateMsg_init(SetFileStateMsg* this);
static inline void SetFileStateMsg_initFromEntryInfoAndState(SetFileStateMsg* this,
      const EntryInfo* entryInfo, uint8_t state);

// virtual functions
extern void SetFileStateMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

struct SetFileStateMsg
{
   NetMessage netMessage;
   uint8_t state;

   const EntryInfo* entryInfoPtr; // not owned by this object
};

extern const struct NetMessageOps SetFileStateMsg_Ops;

void SetFileStateMsg_init(SetFileStateMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_SetFileState, &SetFileStateMsg_Ops);
}

void SetFileStateMsg_initFromEntryInfoAndState(SetFileStateMsg* this, const EntryInfo* entryInfo,
   uint8_t state)
{
   SetFileStateMsg_init(this);
   this->entryInfoPtr = entryInfo;
   this->state = state;
}

#endif // SETFILESTATEMSG_H_