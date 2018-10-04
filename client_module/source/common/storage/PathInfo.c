#include <common/toolkit/Serialization.h>
#include <common/storage/PathInfo.h>
#include <os/OsDeps.h>

/**
 * Serialize into outBuf, 4-byte aligned
 */
void PathInfo_serialize(SerializeCtx* ctx, const PathInfo* this)
{
   // flags
   Serialization_serializeUInt(ctx, this->flags);

   if (this->flags & PATHINFO_FEATURE_ORIG)
   {
      // origParentUID
      Serialization_serializeUInt(ctx, this->origParentUID);

      // origParentEntryID
      Serialization_serializeStrAlign4(ctx, strlen(this->origParentEntryID),
         this->origParentEntryID);
   }
}

/**
 * deserialize the given buffer
 */
bool PathInfo_deserialize(DeserializeCtx* ctx, PathInfo* outThis)
{
   unsigned flags;

   unsigned origParentUID;
   unsigned origParentEntryIDLen;
   const char* origParentEntryID;

   // flags
   if(!Serialization_deserializeUInt(ctx, &flags) )
      return false;

   if (flags & PATHINFO_FEATURE_ORIG)
   {  // file has origParentUID and origParentEntryID
      // origParentUID
      if(!Serialization_deserializeUInt(ctx, &origParentUID) )
         return false;

      // origParentEntryID
      if(!Serialization_deserializeStrAlign4(ctx, &origParentEntryIDLen, &origParentEntryID) )
         return false;
   }
   else
   {  // either a directory or a file stored in old format
      origParentUID = 0;
      origParentEntryID = NULL;
   }

   PathInfo_init(outThis, origParentUID, origParentEntryID, flags);

   return true;
}
