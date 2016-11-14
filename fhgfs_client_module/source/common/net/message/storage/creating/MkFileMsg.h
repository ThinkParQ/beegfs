#ifndef MKFILEMSG_H_
#define MKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <net/filesystem/FhgfsOpsRemoting.h>


#define MKFILEMSG_FLAG_STRIPEHINTS        1 /* msg contains extra stripe hints */
#define MKFILEMSG_FLAG_BUDDYMIRROR_SECOND 2 /* if this message goes to a buddy group, this
                                             * indicates, that it was sent to secondary of group */


struct MkFileMsg;
typedef struct MkFileMsg MkFileMsg;

static inline void MkFileMsg_init(MkFileMsg* this);
static inline void MkFileMsg_initFromEntryInfo(MkFileMsg* this, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo);

// virtual functions
extern void MkFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

// getters & setters
static inline void MkFileMsg_setStripeHints(MkFileMsg* this,
   unsigned numtargets, unsigned chunksize);


/*
 * note: this message supports serialization only, deserialization is not implemented
 */
struct MkFileMsg
{
   NetMessage netMessage;

   unsigned userID;
   unsigned groupID;
   int mode;
   int umask;

   unsigned numtargets;
   unsigned chunksize;

   // for serialization
   const EntryInfo* parentInfoPtr;
   const char* newFileName;
   unsigned newFileNameLen;
   UInt16List* preferredTargets; // not owned by this object!

   // for deserialization
   EntryInfo entryInfo;
   unsigned prefNodesElemNum;
   const char* prefNodesListStart;
   unsigned prefNodesBufLen;
};

extern const struct NetMessageOps MkFileMsg_Ops;

void MkFileMsg_init(MkFileMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_MkFile, &MkFileMsg_Ops);
}

/**
 * @param parentInfo just a reference, so do not free it as long as you use this object!
 */
void MkFileMsg_initFromEntryInfo(MkFileMsg* this, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo)
{
   MkFileMsg_init(this);

   this->parentInfoPtr    = parentInfo;
   this->newFileName      = createInfo->entryName;
   this->newFileNameLen   = strlen(createInfo->entryName);
   this->userID           = createInfo->userID;
   this->groupID          = createInfo->groupID;
   this->mode             = createInfo->mode;
   this->umask            = createInfo->umask;
   this->preferredTargets = createInfo->preferredStorageTargets;
}

/**
 * Note: Adds MKFILEMSG_FLAG_STRIPEHINTS.
 */
void MkFileMsg_setStripeHints(MkFileMsg* this, unsigned numtargets, unsigned chunksize)
{
   NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)this, MKFILEMSG_FLAG_STRIPEHINTS);

   this->numtargets = numtargets;
   this->chunksize = chunksize;
}

#endif /*MKFILEMSG_H_*/
