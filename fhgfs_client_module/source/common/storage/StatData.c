/*
 * Information provided by stat()
 */

#include <common/toolkit/Serialization.h>
#include "StatData.h"

bool StatData_deserialize(DeserializeCtx* ctx, StatData* outThis)
{
   // flags
   if(!Serialization_deserializeUInt(ctx, &outThis->flags) )
      return false;

   // mode
   if(!Serialization_deserializeInt(ctx, &outThis->settableFileAttribs.mode) )
      return false;

   // sumChunkBlocks
   if(!Serialization_deserializeUInt64(ctx, &outThis->numBlocks) )
      return false;

    // creationTime
   if(!Serialization_deserializeInt64(ctx, &outThis->creationTimeSecs) )
      return false;

   // aTime
   if(!Serialization_deserializeInt64(ctx, &outThis->settableFileAttribs.lastAccessTimeSecs) )
      return false;

   // mtime
   if(!Serialization_deserializeInt64(ctx, &outThis->settableFileAttribs.modificationTimeSecs) )
      return false;

   // ctime
   if(!Serialization_deserializeInt64(ctx, &outThis->attribChangeTimeSecs) )
      return false;

   // fileSize
   if(!Serialization_deserializeInt64(ctx, &outThis->fileSize) )
      return false;

   // nlink
   if(!Serialization_deserializeUInt(ctx, &outThis->nlink) )
      return false;

   // contentsVersion
   if(!Serialization_deserializeUInt(ctx, &outThis->contentsVersion) )
      return false;

   // uid
   if(!Serialization_deserializeUInt(ctx, &outThis->settableFileAttribs.userID) )
      return false;

   // gid
   if(!Serialization_deserializeUInt(ctx, &outThis->settableFileAttribs.groupID) )
      return false;

   return true;
}



