#ifndef MKFILEMSG_H_
#define MKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/FileEvent.h>
#include <common/storage/Path.h>
#include <net/filesystem/FhgfsOpsRemoting.h>


#define MKFILEMSG_FLAG_STRIPEHINTS        1 /* msg contains extra stripe hints */
#define MKFILEMSG_FLAG_STORAGEPOOLID      2 /* msg contains a storage pool ID to override parent */
#define MKFILEMSG_FLAG_HAS_EVENT          4 /* contains file event logging information */

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
static inline void MkFileMsg_setStoragePoolId(MkFileMsg* this, StoragePoolId StoragePoolId);


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

   // if this is set, the storage pool of the parent directory is ignored, and this pool is used
   // instead
   StoragePoolId storagePoolId;

   // for serialization
   const EntryInfo* parentInfoPtr;
   const char* newFileName;
   unsigned newFileNameLen;
   UInt16List* preferredTargets; // not owned by this object!
   const struct FileEvent* fileEvent;

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
   this->fileEvent        = createInfo->fileEvent;

   if (createInfo->fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= MKFILEMSG_FLAG_HAS_EVENT;

   if (createInfo->storagePoolId.value != STORAGEPOOLID_INVALIDPOOLID)
      MkFileMsg_setStoragePoolId(this, createInfo->storagePoolId);
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

/**
 * Note: Adds MKFILEMSG_FLAG_STORAGEPOOLID.
 */
void MkFileMsg_setStoragePoolId(MkFileMsg* this, StoragePoolId StoragePoolId)
{
   NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)this, MKFILEMSG_FLAG_STORAGEPOOLID);

   this->storagePoolId = StoragePoolId;
}

#endif /*MKFILEMSG_H_*/
