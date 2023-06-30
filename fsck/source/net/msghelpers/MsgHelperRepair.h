#ifndef MSGHELPERREPAIR_H_
#define MSGHELPERREPAIR_H_

#include <common/Common.h>
#include <common/nodes/Node.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/fsck/FsckChunk.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckDirInode.h>
#include <common/fsck/FsckFsID.h>
#include <common/fsck/FsckDuplicateInodeInfo.h>

class MsgHelperRepair
{
   public:
      static FhgfsOpsErr setNodeState(NumNodeID node, TargetConsistencyState state);

      static void deleteDanglingDirEntries(NumNodeID node, bool isBuddyMirrored,
         FsckDirEntryList* dentries, FsckDirEntryList* failedDeletes,
         std::set<NumNodeID>& secondariesWithRepair);

      static void createDefDirInodes(NumNodeID node, bool isBuddyMirrored,
         const std::vector<std::tuple<std::string, bool>>& entries,
         FsckDirInodeList* createdInodes,
         std::set<NumNodeID>& secondariesWithRepair);

      static void correctInodeOwnersInDentry(NumNodeID node, bool isBuddyMirrored,
         FsckDirEntryList* dentries, NumNodeIDList* owners, FsckDirEntryList* failedCorrections,
         std::set<NumNodeID>& secondariesWithRepair);
      static void correctInodeOwners(NumNodeID node, bool isBuddyMirrored,
         FsckDirInodeList* dirInodes, FsckDirInodeList* failedCorrections,
         std::set<NumNodeID>& secondariesWithRepair);

      static void deleteFiles(NumNodeID node, bool isBuddyMirrored, FsckDirEntryList* dentries,
         FsckDirEntryList* failedDeletes);
      static void deleteChunks(Node& node, FsckChunkList* chunks, FsckChunkList* failedDeletes);

      static NodeHandle referenceLostAndFoundOwner(EntryInfo* outLostAndFoundEntryInfo);
      static bool createLostAndFound(NodeHandle& outReferencedNode,
         EntryInfo& outLostAndFoundEntryInfo);
      static void linkToLostAndFound(Node& lostAndFoundNode, EntryInfo* lostAndFoundInfo,
         FsckDirInodeList* dirInodes, FsckDirInodeList* failedInodes,
         FsckDirEntryList* createdDentries, std::set<NumNodeID>& secondariesWithRepair);
      static void linkToLostAndFound(Node& lostAndFoundNode, EntryInfo* lostAndFoundInfo,
         FsckFileInodeList* fileInodes, FsckFileInodeList* failedInodes,
         FsckDirEntryList* createdDentries, std::set<NumNodeID>& secondariesWithRepair);
      static void createContDirs(NumNodeID node, bool isBuddyMirrored, FsckDirInodeList* inodes,
         StringList* failedCreates, std::set<NumNodeID>& secondariesWithRepair);
      static void updateFileAttribs(NumNodeID node, bool isBuddyMirrored, FsckFileInodeList* inodes,
         FsckFileInodeList* failedUpdates, std::set<NumNodeID>& secondariesWithRepair);
      static void updateDirAttribs(NumNodeID node, bool isBuddyMirrored, FsckDirInodeList* inodes,
         FsckDirInodeList* failedUpdates, std::set<NumNodeID>& secondariesWithRepair);
      static void recreateFsIDs(NumNodeID node, bool isBuddyMirrored,  FsckDirEntryList* dentries,
         FsckDirEntryList* failedEntries, std::set<NumNodeID>& secondariesWithRepair);
      static void recreateDentries(NumNodeID node, bool isBuddyMirrored, FsckFsIDList* fsIDs,
         FsckFsIDList* failedCreates, FsckDirEntryList* createdDentries,
         FsckFileInodeList* createdInodes, std::set<NumNodeID>& secondariesWithRepair);
      static void fixChunkPermissions(Node& node, FsckChunkList& chunkList,
         PathInfoList& pathInfoList, FsckChunkList& failedChunks);
      static bool moveChunk(Node& node, FsckChunk& chunk, const std::string& moveTo,
         bool allowOverwrite);
      static void deleteFileInodes(NumNodeID node, bool isBuddyMirrored, FsckFileInodeList& inodes,
         StringList& failedDeletes, std::set<NumNodeID>& secondariesWithRepair);
      static void deleteDuplicateFileInodes(NumNodeID node, bool isBuddyMirrored,
         FsckDuplicateInodeInfoVector& dupInodes, StringList& failedEntries, std::set<NumNodeID>& secondariesWithRepair);
      static void deinlineFileInode(NumNodeID node, EntryInfo* entryInfo, StringList& failedEntries,
         std::set<NumNodeID>& secondariesWithRepair);


   private:
      MsgHelperRepair() {}

   public:
      // inliners
};

#endif /* MSGHELPERREPAIR_H_ */
