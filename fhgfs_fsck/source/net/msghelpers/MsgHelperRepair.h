#ifndef MSGHELPERREPAIR_H_
#define MSGHELPERREPAIR_H_

#include <common/Common.h>
#include <common/nodes/Node.h>
#include <common/fsck/FsckChunk.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckDirInode.h>
#include <common/fsck/FsckFsID.h>

class MsgHelperRepair
{
   public:
      static void deleteDanglingDirEntries(NumNodeID node, bool isBuddyMirrored,
         FsckDirEntryList* dentries, FsckDirEntryList* failedDeletes);

      static void createDefDirInodes(NumNodeID node, bool isBuddyMirrored,
         const std::vector<std::tuple<std::string, bool>>& entries,
         FsckDirInodeList* createdInodes);

      static void correctInodeOwnersInDentry(NumNodeID node, bool isBuddyMirrored,
         FsckDirEntryList* dentries, NumNodeIDList* owners, FsckDirEntryList* failedCorrections);
      static void correctInodeOwners(NumNodeID node, bool isBuddyMirrored,
         FsckDirInodeList* dirInodes, FsckDirInodeList* failedCorrections);

      static void deleteFiles(NumNodeID node, bool isBuddyMirrored, FsckDirEntryList* dentries,
         FsckDirEntryList* failedDeletes);
      static void deleteChunks(Node& node, FsckChunkList* chunks, FsckChunkList* failedDeletes);

      static NodeHandle referenceLostAndFoundOwner(EntryInfo* outLostAndFoundEntryInfo);
      static bool createLostAndFound(NodeHandle& outReferencedNode,
         EntryInfo& outLostAndFoundEntryInfo);
      static void linkToLostAndFound(Node& lostAndFoundNode, EntryInfo* lostAndFoundInfo,
         FsckDirInodeList* dirInodes, FsckDirInodeList* failedInodes,
         FsckDirEntryList* createdDentries);
      static void linkToLostAndFound(Node& lostAndFoundNode, EntryInfo* lostAndFoundInfo,
         FsckFileInodeList* fileInodes, FsckFileInodeList* failedInodes,
         FsckDirEntryList* createdDentries);
      static void createContDirs(NumNodeID node, bool isBuddyMirrored, FsckDirInodeList* inodes,
         StringList* failedCreates);
      static void updateFileAttribs(NumNodeID node, bool isBuddyMirrored, FsckFileInodeList* inodes,
         FsckFileInodeList* failedUpdates);
      static void updateDirAttribs(NumNodeID node, bool isBuddyMirrored, FsckDirInodeList* inodes,
         FsckDirInodeList* failedUpdates);
      static void recreateFsIDs(NumNodeID node, bool isBuddyMirrored,  FsckDirEntryList* dentries,
         FsckDirEntryList* failedEntries);
      static void recreateDentries(NumNodeID node, bool isBuddyMirrored, FsckFsIDList* fsIDs,
         FsckFsIDList* failedCreates, FsckDirEntryList* createdDentries,
         FsckFileInodeList* createdInodes);
      static void fixChunkPermissions(Node& node, FsckChunkList& chunkList,
         PathInfoList& pathInfoList, FsckChunkList& failedChunks);
      static bool moveChunk(Node& node, FsckChunk& chunk, const std::string& moveTo,
         bool allowOverwrite);
      static void deleteFileInodes(NumNodeID node, bool isBuddyMirrored, FsckFileInodeList& inodes,
         StringList& failedDeletes);


   private:
      MsgHelperRepair() {}

   public:
      // inliners
};

#endif /* MSGHELPERREPAIR_H_ */
