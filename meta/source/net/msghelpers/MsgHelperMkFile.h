#pragma once


#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include <storage/MetaStore.h>
#include <storage/MkFileDetails.h>


struct MkFileDetails; // forward declaration

/**
 * Default class to create meta-data files (including inodes and directories).
 */
class MsgHelperMkFile
{
   public:
      static FhgfsOpsErr mkFile(DirInode& parentDir, MkFileDetails* mkDetails,
         const UInt16List* preferredTargets, const unsigned numtargets, const unsigned chunksize,
         StripePattern* stripePattern, RemoteStorageTarget* rstInfo, EntryInfo* outEntryInfo,
         FileInodeStoreData* outInodeData, StoragePoolId storagePoolId = StoragePoolStore::INVALID_POOL_ID);

   private:
      MsgHelperMkFile() {}

};


