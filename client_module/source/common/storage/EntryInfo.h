/*
 * class EntryInfo - required information to find an inode or chunk files
 *
 * NOTE: If you change this file, do not forget to adjust commons EntryInfo.h
 */

#ifndef ENTRYINFO_H_
#define ENTRYINFO_H_

#include <common/nodes/NumNodeID.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/SerializationTypes.h>

#define ENTRYINFO_FEATURE_INLINED       1 // indicate inlined inode, might be outdated
#define ENTRYINFO_FEATURE_BUDDYMIRRORED 2 // entry is buddy mirrored

#define EntryInfo_PARENT_ID_UNKNOWN    "unknown"

struct EntryInfo;
typedef struct EntryInfo EntryInfo;


static inline void EntryInfo_uninit(EntryInfo* this);

static inline void EntryInfo_dup(const EntryInfo *inEntryInfo, EntryInfo *outEntryInfo);

static inline void EntryInfo_update(EntryInfo* this, const EntryInfo* newEntryInfo);
static inline void EntryInfo_updateParentEntryID(EntryInfo *this, const char *newParentID);
static inline void EntryInfo_updateSetParentEntryID(EntryInfo *this, const char *newParentID);

static inline char const* EntryInfo_getParentEntryID(const EntryInfo* this);
static inline char const* EntryInfo_getEntryID(const EntryInfo* this);
static inline void EntryInfo_updateFileName(EntryInfo *this, const char* newFileName);
static inline bool EntryInfo_getIsBuddyMirrored(const EntryInfo *this);

extern void EntryInfo_serialize(SerializeCtx* ctx, const EntryInfo* this);
extern bool EntryInfo_deserialize(DeserializeCtx* ctx, EntryInfo* outThis);


typedef struct NodeOrGroup
{
   union
   {
      NumNodeID node;
      uint32_t group;
   };
   bool isGroup;
} NodeOrGroup;

static inline NodeOrGroup NodeOrGroup_fromNode(NumNodeID node)
{
   NodeOrGroup result = { .isGroup = false };
   result.node = node;
   return result;
}

static inline NodeOrGroup NodeOrGroup_fromGroup(uint32_t group)
{
   NodeOrGroup result = { .isGroup = true };
   result.group = group;
   return result;
}

static inline bool NodeOrGroup_valid(NodeOrGroup id)
{
   return id.group != 0;
}

/**
 * minimal information about an entry (file/directory/...)
 */
struct EntryInfo
{
   // serialization will work as long as the elements in here have the same size and
   // underlying type(!)
   NodeOrGroup owner;
   const char* parentEntryID;
   const char* entryID;
   const char* fileName;
   DirEntryType entryType;
   int featureFlags;
};




/**
 * Main initialization function for EntryInfo, should typically be used
 *
 * @param parentEntryID will be free'd on uninit
 * @param entryID will be free'd on uninit
 * @param fileName will be free'd on uninit
 */
static inline void EntryInfo_init(EntryInfo* this, NodeOrGroup owner,
   const char* parentEntryID, const char* entryID, const char* fileName, DirEntryType entryType,
   int featureFlags)
{
   this->owner = owner;
   this->parentEntryID = parentEntryID;
   this->entryID = entryID;
   this->fileName = fileName;
   this->entryType = entryType;
   this->featureFlags = featureFlags | (owner.isGroup ? ENTRYINFO_FEATURE_BUDDYMIRRORED : 0);
}

/**
 * unitialize the object
 */
void EntryInfo_uninit(EntryInfo* this)
{
   kfree(this->parentEntryID);
   kfree(this->entryID);
   kfree(this->fileName);
}

/**
 * Duplicate inEntryInfo to outEntryInfo, also allocate memory for strings
 */
void EntryInfo_dup(const EntryInfo *inEntryInfo, EntryInfo *outEntryInfo)
{
   *outEntryInfo = *inEntryInfo;

   outEntryInfo->parentEntryID = StringTk_strDup(outEntryInfo->parentEntryID);
   outEntryInfo->entryID = StringTk_strDup(outEntryInfo->entryID);
   outEntryInfo->fileName = StringTk_strDup(outEntryInfo->fileName);
}


/**
 * Update our EntryInfo (this) with a new EntryInfo
 *
 * Note: newEntryInfo must not be used any more by the caller
 * Note: This does not update entryID and entryType as these values cannot change. If we would
 *       update entryID we would also increase locking overhead.
 */
void EntryInfo_update(EntryInfo* this, const EntryInfo* newEntryInfo)
{
   kfree(this->parentEntryID);
   kfree(this->fileName);

   this->parentEntryID = newEntryInfo->parentEntryID;
   this->fileName = newEntryInfo->fileName;
   this->featureFlags = newEntryInfo->featureFlags;

   if (!DirEntryType_ISDIR(this->entryType) )
   {  // only update the owner if it is not a directory
      this->owner = newEntryInfo->owner;
   }

   kfree(newEntryInfo->entryID);
}


void EntryInfo_updateParentEntryID(EntryInfo *this, const char *newParentID)
{
   kfree(this->parentEntryID);
   this->parentEntryID = StringTk_strDup(newParentID);
}

/**
 * Note: This sets to newParentID, the caller must not use newParentID
 */
void EntryInfo_updateSetParentEntryID(EntryInfo *this, const char *newParentID)
{
   kfree(this->parentEntryID);
   this->parentEntryID = newParentID;
}

char const* EntryInfo_getParentEntryID(const EntryInfo* this)
{
   return this->parentEntryID;
}

char const* EntryInfo_getEntryID(const EntryInfo* this)
{
   return this->entryID;
}

void EntryInfo_updateFileName(EntryInfo *this, const char* newFileName)
{
   kfree(this->fileName);
   this->fileName =  StringTk_strDup(newFileName);
}

bool EntryInfo_getIsBuddyMirrored(const EntryInfo *this)
{
   return (this->featureFlags & ENTRYINFO_FEATURE_BUDDYMIRRORED);
}


/* these two are mainly for logging. */
static inline uint32_t EntryInfo_getOwner(const EntryInfo* this)
{
   /* owner.node and owner.group are required to be the same type and size */
   return this->owner.group;
}

static inline const char* EntryInfo_getOwnerFlag(const EntryInfo* this)
{
   if (this->featureFlags & ENTRYINFO_FEATURE_BUDDYMIRRORED)
      return "g";
   else
      return "";
}


#endif /* ENTRYINFO_H_ */
