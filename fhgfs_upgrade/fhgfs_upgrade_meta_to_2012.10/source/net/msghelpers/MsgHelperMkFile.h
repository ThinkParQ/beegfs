#ifndef MSGHELPERMKFILE_H_
#define MSGHELPERMKFILE_H_


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
      static FhgfsOpsErr mkFile(EntryInfo* parentInfo, MkFileDetails* mkDetails,
         UInt16List* preferredTargets, EntryInfo* outEntryInfo, DentryInodeMeta* outInodeData);

   private:
      MsgHelperMkFile() {}

};


#endif /* MSGHELPERMKFILE_H_ */
