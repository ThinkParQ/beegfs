#ifndef MSGHELPERSTAT_H_
#define MSGHELPERSTAT_H_

#include <common/Common.h>
#include <storage/MetaStore.h>
#include <storage/MetadataEx.h>


class MsgHelperStat
{
   public:
      static FhgfsOpsErr stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData);
      static FhgfsOpsErr refreshDynAttribs(EntryInfo* entryInfo, bool makePersistent);


   private:
      MsgHelperStat() {}

      static FhgfsOpsErr refreshDynAttribsSequential(FileInode* file, std::string entryID);
      static FhgfsOpsErr refreshDynAttribsParallel(FileInode* file, std::string entryID);


   public:
      // inliners
};

#endif /* MSGHELPERSTAT_H_ */
