#include <common/toolkit/Serialization.h>
#include <common/storage/EntryInfo.h>
#include <os/OsDeps.h>

#define MIN_ENTRY_ID_LEN 1 // usually A-B-C, but also "root" and "disposal"

/**
 * Serialize into outBuf, 4-byte aligned
 */
void EntryInfo_serialize(SerializeCtx* ctx, const EntryInfo* this)
{
   // DirEntryType
   Serialization_serializeUInt(ctx, this->entryType);

   // featureFlags
   Serialization_serializeInt(ctx, this->featureFlags);

   // parentEntryID
   Serialization_serializeStrAlign4(ctx, strlen(this->parentEntryID), this->parentEntryID);

   if (unlikely(strlen(this->entryID) < MIN_ENTRY_ID_LEN) )
   {
      printk_fhgfs(KERN_WARNING, "EntryID too small!\n"); // server side deserialization will fail
      dump_stack();
   }

   // entryID
   Serialization_serializeStrAlign4(ctx, strlen(this->entryID), this->entryID);

   // fileName
   Serialization_serializeStrAlign4(ctx, strlen(this->fileName), this->fileName);

   // ownerNodeID
   // also serializes owner.group, if buddymirrored. both MUST have the same size and underlying
   // type!
   BUILD_BUG_ON(
         sizeof(this->owner.node) != sizeof(this->owner.group)
            || !__builtin_types_compatible_p(
               __typeof(this->owner.node.value), __typeof(this->owner.group)));
   NumNodeID_serialize(ctx, &this->owner.node);

   // padding for 4-byte alignment
   Serialization_serializeUShort(ctx, 0);
}

/**
 * deserialize the given buffer
 */
bool EntryInfo_deserialize(DeserializeCtx* ctx, EntryInfo* outThis)
{
   unsigned parentEntryIDLen;
   unsigned entryIDLen;
   unsigned fileNameLen;
   unsigned entryType;
   unsigned short padding;

   if (!Serialization_deserializeUInt(ctx, &entryType))
      return false;
   outThis->entryType = (DirEntryType) entryType;

   if (!Serialization_deserializeInt(ctx, &outThis->featureFlags)
         || !Serialization_deserializeStrAlign4(ctx, &parentEntryIDLen, &outThis->parentEntryID)
         || !Serialization_deserializeStrAlign4(ctx, &entryIDLen, &outThis->entryID)
         || !Serialization_deserializeStrAlign4(ctx, &fileNameLen, &outThis->fileName))
      return false;

   outThis->owner.isGroup = outThis->featureFlags & ENTRYINFO_FEATURE_BUDDYMIRRORED;

   // also deserializes owner.group, if buddy mirrored.
   if (!NumNodeID_deserialize(ctx, &outThis->owner.node))
      return false;

   // padding for 4-byte alignment
   if(!Serialization_deserializeUShort(ctx, &padding))
      return false;

   // Note: the root ("/") has parentEntryID = ""
   if (unlikely((parentEntryIDLen < MIN_ENTRY_ID_LEN) && (parentEntryIDLen > 0)))
      return false;

   if (unlikely(entryIDLen < MIN_ENTRY_ID_LEN))
      return false;

   return true;
}
