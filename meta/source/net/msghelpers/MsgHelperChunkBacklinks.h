#ifndef MSGHELPERCHUNKBACKLINKS_H
#define MSGHELPERCHUNKBACKLINKS_H

#include <common/storage/EntryInfo.h>
#include <common/storage/StorageErrors.h>
#include <storage/DirInode.h>

class MsgHelperChunkBacklinks
{
   public:
      static FhgfsOpsErr updateBacklink(const std::string& parentID, bool isParentBuddyMirrored,
         const std::string& name);
      static FhgfsOpsErr updateBacklink(DirInode& parent, const std::string& name);
      static FhgfsOpsErr updateBacklink(EntryInfo& entryInfo);

   private:
      MsgHelperChunkBacklinks() {}

   public:
};

#endif /* MSGHELPERCHUNKBACKLINKS_H */
