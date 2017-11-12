#ifndef LOOKUPINTENTMSG_H_
#define LOOKUPINTENTMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>


/**
 * This message supports only serialization; deserialization is not implemented!
 */


/* intentFlags as payload
   Keep these flags in sync with the userspace msg flags:
   fhgfs_common/source/common/net/message/storage/LookupIntentMsg.h */
#define LOOKUPINTENTMSG_FLAG_REVALIDATE         1 /* revalidate entry, cancel if invalid */
#define LOOKUPINTENTMSG_FLAG_CREATE             2 /* create file */
#define LOOKUPINTENTMSG_FLAG_CREATEEXCLUSIVE    4 /* exclusive file creation */
#define LOOKUPINTENTMSG_FLAG_OPEN               8 /* open file */
#define LOOKUPINTENTMSG_FLAG_STAT              16 /* stat file */


// feature flags as header flags
#define LOOKUPINTENTMSG_FLAG_USE_QUOTA          1 /* if the message contains quota information */
#define LOOKUPINTENTMSG_FLAG_BUDDYMIRROR        2
#define LOOKUPINTENTMSG_FLAG_BUDDYMIRROR_SECOND 4
#define LOOKUPINTENTMSG_FLAG_HAS_EVENT          8 /* contains file event logging information */


struct LookupIntentMsg;
typedef struct LookupIntentMsg LookupIntentMsg;


static inline void LookupIntentMsg_init(LookupIntentMsg* this);
static inline void LookupIntentMsg_initFromName(LookupIntentMsg* this, const EntryInfo* parentInfo,
   const char* entryName);
static inline void LookupIntentMsg_initFromEntryInfo(LookupIntentMsg* this,
   const EntryInfo* parentInfo, const char* entryName, const EntryInfo* entryInfo);

// virtual functions
extern void LookupIntentMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

// inliners
static inline void LookupIntentMsg_addIntentCreate(LookupIntentMsg* this,
   unsigned userID, unsigned groupID, int mode, int umask, UInt16List* preferredTargets,
   const struct FileEvent* fileEvent);
static inline void LookupIntentMsg_addIntentCreateExclusive(LookupIntentMsg* this);
static inline void LookupIntentMsg_addIntentOpen(LookupIntentMsg* this, NumNodeID clientNumID,
   unsigned accessFlags);
static inline void LookupIntentMsg_addIntentStat(LookupIntentMsg* this);


struct LookupIntentMsg
{
   NetMessage netMessage;

   int intentFlags; // combination of LOOKUPINTENTMSG_FLAG_...

   const char* entryName; // (lookup data), set for all but revalidate
   unsigned entryNameLen; // (lookup data), set for all but revalidate

   const EntryInfo* entryInfoPtr; // (revalidation data)

   unsigned userID; // (file creation data)
   unsigned groupID; // (file creation data)
   int mode; // file creation mode permission bits  (file creation data)
   int umask; // umask of the context (file creation data)
   const struct FileEvent* fileEvent;

   NumNodeID clientNumID; // (file open data)
   unsigned accessFlags; // OPENFILE_ACCESS_... flags (file open data)

   // for serialization
   const EntryInfo* parentInfoPtr; // not owned by this object (lookup/open/creation data)
   UInt16List* preferredTargets; // not owned by this object! (file creation data)
};

extern const struct NetMessageOps LookupIntentMsg_Ops;

void LookupIntentMsg_init(LookupIntentMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_LookupIntent, &LookupIntentMsg_Ops);
   this->fileEvent = NULL;
}

/**
 * This just prepares the basic lookup. Use the additional addIntent...() methods to do
 * more than just the lookup.
 *
 * @param parentEntryID just a reference, so do not free it as long as you use this object!
 * @param entryName just a reference, so do not free it as long as you use this object!
 */
void LookupIntentMsg_initFromName(LookupIntentMsg* this, const EntryInfo* parentInfo,
   const char* entryName)
{
   LookupIntentMsg_init(this);

   this->intentFlags = 0;

   this->parentInfoPtr = parentInfo;

   this->entryName = entryName;
   this->entryNameLen = strlen(entryName);
}

/**
 * Initialize from entryInfo - supposed to be used for revalidate intent
 */
void LookupIntentMsg_initFromEntryInfo(LookupIntentMsg* this, const EntryInfo* parentInfo,
   const char* entryName, const EntryInfo* entryInfo)
{
   LookupIntentMsg_init(this);

   this->intentFlags = LOOKUPINTENTMSG_FLAG_REVALIDATE;

   this->parentInfoPtr = parentInfo;

   this->entryName = entryName;
   this->entryNameLen = strlen(entryName);

   this->entryInfoPtr = entryInfo;
}

void LookupIntentMsg_addIntentCreate(LookupIntentMsg* this,
   unsigned userID, unsigned groupID, int mode, int umask, UInt16List* preferredTargets,
   const struct FileEvent* fileEvent)
{
   this->intentFlags |= LOOKUPINTENTMSG_FLAG_CREATE;

   this->userID = userID;
   this->groupID = groupID;
   this->mode = mode;
   this->umask = umask;
   this->preferredTargets = preferredTargets;
   this->fileEvent = fileEvent;
   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= LOOKUPINTENTMSG_FLAG_HAS_EVENT;
}

void LookupIntentMsg_addIntentCreateExclusive(LookupIntentMsg* this)
{
   this->intentFlags |= LOOKUPINTENTMSG_FLAG_CREATEEXCLUSIVE;
}

/**
 * @param accessFlags OPENFILE_ACCESS_... flags
 */
void LookupIntentMsg_addIntentOpen(LookupIntentMsg* this, NumNodeID clientNumID,
   unsigned accessFlags)
{
   this->intentFlags |= LOOKUPINTENTMSG_FLAG_OPEN;

   this->clientNumID = clientNumID;

   this->accessFlags = accessFlags;
}

void LookupIntentMsg_addIntentStat(LookupIntentMsg* this)
{
   this->intentFlags |= LOOKUPINTENTMSG_FLAG_STAT;
}



#endif /* LOOKUPINTENTMSG_H_ */
