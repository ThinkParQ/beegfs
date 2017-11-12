#ifndef MSGHELPERCLOSE_H_
#define MSGHELPERCLOSE_H_

#include <common/Common.h>
#include <storage/MetaStore.h>


class MsgHelperClose
{
   public:
      static FhgfsOpsErr closeFile(std::string sessionID, std::string fileHandleID,
         EntryInfo* entryInfo, int maxUsedNodeIndex, std::string* outFileID,
         bool* unlinkDisposalFile);
      static FhgfsOpsErr closeChunkFile(std::string sessionID, std::string fileHandleID,
         int maxUsedNodeIndex, FileInode* file);
      static FhgfsOpsErr unlinkDisposableFile(std::string fileID);


   private:
      MsgHelperClose() {}

      static FhgfsOpsErr closeSessionFile(std::string sessionID, std::string fileHandleID,
         EntryInfo* entryInfo, unsigned* outAccessFlags, FileInode** outCloseFile);
      static FhgfsOpsErr closeChunkFileSequential(std::string sessionID, std::string fileHandleID,
         int maxUsedNodeIndex, FileInode* file);
      static FhgfsOpsErr closeChunkFileParallel(std::string sessionID, std::string fileHandleID,
         int maxUsedNodeIndex, FileInode* file);


   public:
      // inliners
};

#endif /* MSGHELPERCLOSE_H_ */
