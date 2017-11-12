#ifndef MKDIRMSG_H_
#define MKDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <net/filesystem/FhgfsOpsRemoting.h>


/**
 * this message supports only serialization, deserialization is not implemented.
 */

#define MKDIRMSG_FLAG_NOMIRROR            1 /* do not use mirror setting from parent
                                             * (i.e. do not mirror) */
#define MKDIRMSG_FLAG_BUDDYMIRROR_SECOND  2 /* if this message goes to a buddy group, this
                                             * indicates, that it was sent to secondary of group */
#define MKDIRMSG_FLAG_HAS_EVENT           4 /* contains file event logging information */

struct MkDirMsg;
typedef struct MkDirMsg MkDirMsg;

static inline void MkDirMsg_init(MkDirMsg* this);
static inline void MkDirMsg_initFromEntryInfo(MkDirMsg* this, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo);

// virtual functions
extern void MkDirMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);



struct MkDirMsg
{
   NetMessage netMessage;

   unsigned userID;
   unsigned groupID;
   int mode;
   int umask;

   // for serialization
   const EntryInfo* parentInfo; // not owned by this object!

   const char* newDirName;
   unsigned newDirNameLen;

   UInt16List* preferredNodes; // not owned by this object!
   const struct FileEvent* fileEvent;
};

extern const struct NetMessageOps MkDirMsg_Ops;

void MkDirMsg_init(MkDirMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_MkDir, &MkDirMsg_Ops);
}

/**
 * @param path just a reference, so do not free it as long as you use this object!
 */
void MkDirMsg_initFromEntryInfo(MkDirMsg* this, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo)
{
   MkDirMsg_init(this);

   this->parentInfo = parentInfo;
   this->newDirName = createInfo->entryName;
   this->newDirNameLen = strlen(createInfo->entryName);

   this->userID  = createInfo->userID;
   this->groupID = createInfo->groupID;
   this->mode    = createInfo->mode;
   this->umask   = createInfo->umask;
   this->preferredNodes = createInfo->preferredMetaTargets;
   this->fileEvent = createInfo->fileEvent;

   if (createInfo->fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= MKDIRMSG_FLAG_HAS_EVENT;
}

#endif /*MKDIRMSG_H_*/
