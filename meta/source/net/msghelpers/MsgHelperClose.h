#ifndef MSGHELPERCLOSE_H_
#define MSGHELPERCLOSE_H_

#include <common/Common.h>
#include <storage/MetaStore.h>

class MsgHelperClose
{
   public:
      static FhgfsOpsErr closeFile(const NumNodeID sessionID, const std::string& fileHandleID,
         EntryInfo* entryInfo, int maxUsedNodeIndex, unsigned msgUserID,
         bool* outUnlinkDisposalFile, bool* outModificationEventsMissed,
         DynamicFileAttribsVec* dynAttribs = NULL, MirroredTimestamps* timestamps = NULL);
      static FhgfsOpsErr closeSessionFile(const NumNodeID sessionID, const std::string& fileHandleID,
         EntryInfo* entryInfo, unsigned* outAccessFlags, MetaFileHandle& outCloseInode);
      static FhgfsOpsErr closeChunkFile(const NumNodeID sessionID,
         const std::string& fileHandleID, int maxUsedNodeIndex, FileInode& inode,
         EntryInfo *entryInfo, unsigned msgUserID, DynamicFileAttribsVec* dynAttribs = NULL);
      static FhgfsOpsErr unlinkDisposableFile(const std::string& fileID, unsigned msgUserID,
            bool isBuddyMirrored);

   private:
      MsgHelperClose()
      {
      }

      static FhgfsOpsErr closeChunkFileSequential(const NumNodeID sessionID,
         const std::string& fileHandleID, int maxUsedNodeIndex, FileInode& inode,
         EntryInfo *entryInfo, unsigned msgUserID, DynamicFileAttribsVec* dynAttribs);
      static FhgfsOpsErr closeChunkFileParallel(const NumNodeID sessionID,
         const std::string& fileHandleID, int maxUsedNodeIndex, FileInode& inode,
         EntryInfo* entryInfo, unsigned msgUserID, DynamicFileAttribsVec* dynAttribs);

   public:
      // inliners
};

#endif /* MSGHELPERCLOSE_H_ */
