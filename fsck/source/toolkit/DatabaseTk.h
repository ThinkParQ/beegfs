#ifndef DATABASETK_H_
#define DATABASETK_H_

#include <common/fsck/FsckChunk.h>
#include <common/fsck/FsckContDir.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckDirInode.h>
#include <common/fsck/FsckFileInode.h>
#include <common/fsck/FsckFsID.h>
#include <common/fsck/FsckModificationEvent.h>
#include <common/fsck/FsckTargetID.h>

#include <database/Chunk.h>
#include <database/ContDir.h>
#include <database/DirEntry.h>
#include <database/DirInode.h>
#include <database/FileInode.h>
#include <database/FsID.h>

class FsckDB;

class DatabaseTk
{
   public:
      DatabaseTk();
      virtual ~DatabaseTk();

      static FsckDirEntry createDummyFsckDirEntry(
         FsckDirEntryType entryType = FsckDirEntryType_REGULARFILE);
      static void createDummyFsckDirEntries(uint amount, FsckDirEntryList* outList,
         FsckDirEntryType entryType = FsckDirEntryType_REGULARFILE);
      static void createDummyFsckDirEntries(uint from, uint amount, FsckDirEntryList* outList,
         FsckDirEntryType entryType = FsckDirEntryType_REGULARFILE);


      static FsckFileInode createDummyFsckFileInode();
      static void createDummyFsckFileInodes(uint amount, FsckFileInodeList* outList);
      static void createDummyFsckFileInodes(uint from, uint amount, FsckFileInodeList* outList);

      static FsckDirInode createDummyFsckDirInode();
      static void createDummyFsckDirInodes(uint amount, FsckDirInodeList* outList);
      static void createDummyFsckDirInodes(uint from, uint amount, FsckDirInodeList* outList);

      static FsckChunk createDummyFsckChunk();
      static void createDummyFsckChunks(uint amount, FsckChunkList* outList);
      static void createDummyFsckChunks(uint amount, uint numTargets, FsckChunkList* outList);
      static void createDummyFsckChunks(uint from, uint amount, uint numTargets,
         FsckChunkList* outList);

      static FsckContDir createDummyFsckContDir();
      static void createDummyFsckContDirs(uint amount, FsckContDirList* outList);
      static void createDummyFsckContDirs(uint from, uint amount, FsckContDirList* outList);

      static FsckFsID createDummyFsckFsID();
      static void createDummyFsckFsIDs(uint amount, FsckFsIDList* outList);
      static void createDummyFsckFsIDs(uint from, uint amount, FsckFsIDList* outList);

      static std::string calculateExpectedChunkPath(std::string entryID, unsigned origParentUID,
         std::string origParentEntryID, int pathInfoFlags);
};

#endif /* DATABASETK_H_ */
