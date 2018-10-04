/*
 * class PathInfo - extra information how to find chunk files (or later on inode files)
 *
 * NOTE: If you change this file, do not forget to adjust commons PathInfo.h
 */

#ifndef PATHINFO_H_
#define PATHINFO_H_

#include <common/storage/PathInfo.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/SerializationTypes.h>
#include <common/toolkit/StringTk.h>

#define PATHINFO_FEATURE_ORIG         1 /* inidicate chunks are stored with origParentUID and
                                          * and origParentEntryID */
#define PATHINFO_FEATURE_ORIG_UNKNOWN 2 /* indicates FEATURE_ORIG is unknown and needs to be
                                          * requested from the meta-inode */

struct PathInfo;
typedef struct PathInfo PathInfo;


static inline void PathInfo_init(PathInfo *this,
   unsigned origParentUID, const char* origParentEntryID, unsigned flags);
static inline void PathInfo_uninit(PathInfo* this);

static inline void PathInfo_dup(const PathInfo *inPathInfo, PathInfo *outPathInfo);
static inline void PathInfo_update(PathInfo* this, const PathInfo* newPathInfo);

static inline void PathInfo_setOrigUID(PathInfo* this, unsigned origParentUID);
static inline void PathInfo_setOrigParentEntryID(PathInfo* this, const char* origParentEntryID);
static inline void PathInfo_setFlags(PathInfo *this, unsigned flags);

extern void PathInfo_serialize(SerializeCtx* ctx, const PathInfo* this);
extern bool PathInfo_deserialize(DeserializeCtx* ctx, PathInfo* outThis);


/**
 * minimal information about an entry (file/directory/...)
 * note: In order to properly initialize this struct, PathInfo_init() has to be called. This
 *       is also the only function, which ever should write to the write-unprotected '_' functions.
 *       Other code is supposed to use the function without the underscore.
 */
struct PathInfo
{
   union
   {
         const unsigned flags;          // additional flags (e.g. PATHINFO_FEATURE_INLINED)
         unsigned _flags;
   };

   union
   {
         const unsigned origParentUID; // UID who created the file, only set for FileInodes
         unsigned _origParentUID;
   };

   union
   {
         // ID of the dir in which the file was created in. Only set for FileInodes
         char const* const origParentEntryID;
         char* _origParentEntryID;
   };
};




/**
 * Main initialization function for PathInfo, should typically be used
 *
 * @param origParentEntryID will be free'd on uninit
 */
void PathInfo_init(PathInfo* this, unsigned origParentUID, const char* origParentEntryID, unsigned flags)
{
   PathInfo_setOrigUID(this, origParentUID);
   PathInfo_setOrigParentEntryID(this, origParentEntryID);
   PathInfo_setFlags(this, flags);
}

/**
 * unitialize the object
 */
void PathInfo_uninit(PathInfo* this)
{
   if (this->flags & PATHINFO_FEATURE_ORIG)
      kfree(this->origParentEntryID);
}

/**
 * Duplicate inPathInfo to outPathInfo, also allocate memory for strings
 */
void PathInfo_dup(const PathInfo* inPathInfo, PathInfo* outPathInfo)
{
   int outFlags                     = inPathInfo->flags;

   unsigned outOrigUID              = inPathInfo->origParentUID;
   const char* outOrigParentEntryID;

   if (outFlags & PATHINFO_FEATURE_ORIG)
      outOrigParentEntryID           = StringTk_strDup(inPathInfo->origParentEntryID);
   else
      outOrigParentEntryID           = NULL;

   PathInfo_init(outPathInfo, outOrigUID, outOrigParentEntryID, outFlags);
}

/**
 * Update an existing PathInfo
 */
void PathInfo_update(PathInfo* this, const PathInfo* newPathInfo)
{
   bool needUpdate = false;

   if (this->flags != newPathInfo->flags)
      needUpdate = true;
   else
   if (this->origParentUID != newPathInfo->origParentUID)
      needUpdate = true;
   else
   if ( (this->origParentEntryID && newPathInfo->origParentEntryID) &&
        (strcmp(this->origParentEntryID, newPathInfo->origParentEntryID) != 0 ) )
   {
      // note: no need for other tests in the if-condition, as flags would be different then.
         needUpdate = true;
   }

   if (needUpdate)
   {
      PathInfo_uninit(this);
      PathInfo_dup(newPathInfo, this);
   }
}

void PathInfo_setFlags(PathInfo* this, unsigned flags)
{
   this->_flags = flags;
}

void PathInfo_setOrigUID(PathInfo* this, unsigned origParentUID)
{
   this->_origParentUID = origParentUID;
}

void PathInfo_setOrigParentEntryID(PathInfo* this, const char* origParentEntryID)
{
   this->_origParentEntryID = (char *) origParentEntryID;
}

#endif /* PATHINFO_H_ */
