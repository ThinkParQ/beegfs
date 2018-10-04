#ifndef MSGHELPERSTAT_H_
#define MSGHELPERSTAT_H_

#include <common/Common.h>
#include <storage/MetaStore.h>
#include <storage/MetadataEx.h>


class MsgHelperStat
{
   public:
      static FhgfsOpsErr stat(EntryInfo* entryInfo, bool loadFromDisk, unsigned msgUserID,
         StatData& outStatData, NumNodeID* outParentNodeID = NULL,
         std::string* outParentEntryID = NULL);
      static FhgfsOpsErr refreshDynAttribs(EntryInfo* entryInfo, bool makePersistent,
         unsigned msgUserID);


   private:
      MsgHelperStat() {}

      static FhgfsOpsErr refreshDynAttribsSequential(FileInode& inode, const std::string& entryID,
         unsigned msgUserID);
      static FhgfsOpsErr refreshDynAttribsParallel(FileInode& inode, const std::string& entryID,
         unsigned msgUserID);


   public:
      // inliners
};

#endif /* MSGHELPERSTAT_H_ */
