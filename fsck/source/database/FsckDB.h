/*
 * This class is intended to be the interface to the sqlite DB. It can be created once in the
 * applicationm, and can then be used in all threads, as sqlite claims to be thread-safe in
 * "serialized" mode (which is the default, http://sqlite.org/threadsafe.html).
 */

#ifndef FSCKDB_H_
#define FSCKDB_H_

#include <common/app/log/LogContext.h>
#include <common/Common.h>
#include <common/fsck/FsckChunk.h>
#include <common/fsck/FsckContDir.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckDirInode.h>
#include <common/fsck/FsckFileInode.h>
#include <common/fsck/FsckFsID.h>
#include <common/fsck/FsckTargetID.h>
#include <common/fsck/FsckDuplicateInodeInfo.h>
#include <common/nodes/TargetMapper.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/StorageDefinitions.h>
#include <database/ContDir.h>
#include <database/Cursor.h>
#include <database/DirEntry.h>
#include <database/DirInode.h>
#include <database/DiskList.h>
#include <database/EntryID.h>
#include <database/FileInode.h>
#include <database/Filter.h>
#include <database/VectorSource.h>
#include <toolkit/FsckDefinitions.h>

#include <boost/scoped_ptr.hpp>

class FsckDBDentryTable;
class FsckDBFileInodesTable;
class FsckDBDirInodesTable;
class FsckDBChunksTable;
class FsckDBContDirsTable;
class FsckDBFsIDsTable;
class FsckDBUsedTargetIDsTable;
class FsckDBModificationEventsTable;
class FsckDBDentryErrorTable;
class FsckDBDirInodeErrorTable;
class FsckDBFsIDErrorTable;
class FsckDBFileInodeErrorTable;
class FsckDBChunkErrorTable;
class FsckDBContDirErrorTable;
class FsckDBStripeTargetErrorTable;

namespace checks {

struct InodeAttribs
{
   uint64_t size;
   uint64_t nlinks;
};

struct OptionalInodeAttribs
{
   boost::optional<uint64_t> size;
   boost::optional<uint64_t> nlinks;

   void reset() {
       size.reset();
       nlinks.reset();
   }
};

typedef std::pair<db::EntryID, std::set<FsckDuplicateInodeInfo>> DuplicatedInode;
}

class FsckDB
{
   friend class TestDatabase;

   public:
      // FsckDB.cpp
      FsckDB(const std::string& databasePath, size_t fragmentSize, size_t nameCacheLimit,
         bool allowCreate);

      void clear();

      // FsckDBChecks.cpp
      Cursor<checks::DuplicatedInode> findDuplicateInodeIDs();
      Cursor<std::list<FsckChunk> > findDuplicateChunks();
      Cursor<std::list<db::ContDir>> findDuplicateContDirs();

      Cursor<db::DirEntry> findMismirroredDentries();
      Cursor<db::DirInode> findMismirroredDirectories();
      Cursor<db::FileInode> findMismirroredFiles();

      Cursor<db::DirEntry> findDanglingDirEntries();
      Cursor<db::DirEntry> findDirEntriesWithBrokenByIDFile();
      Cursor<FsckFsID> findOrphanedFsIDFiles();
      Cursor<FsckDirInode> findInodesWithWrongOwner();
      Cursor<std::pair<db::DirEntry, NumNodeID> > findDirEntriesWithWrongOwner();
      Cursor<FsckDirInode> findOrphanedDirInodes();
      Cursor<FsckFileInode> findOrphanedFileInodes();
      Cursor<FsckChunk> findOrphanedChunks();

      Cursor<FsckDirInode> findInodesWithoutContDir();

      Cursor<FsckContDir> findOrphanedContDirs();

      Cursor<std::pair<FsckFileInode, checks::OptionalInodeAttribs> > findWrongInodeFileAttribs();

      Cursor<std::pair<FsckDirInode, checks::InodeAttribs> > findWrongInodeDirAttribs();

      Cursor<db::DirEntry> findFilesWithMissingStripeTargets(TargetMapper* targetMapper,
         MirrorBuddyGroupMapper* buddyGroupMapper);
      Cursor<std::pair<FsckChunk, FsckFileInode> > findChunksWithWrongPermissions();
      Cursor<std::pair<FsckChunk, FsckFileInode> > findChunksInWrongPath();
      Cursor<db::FileInode> findFilesWithMultipleHardlinks();

   private:
      LogContext log;
      std::string databasePath;

      boost::scoped_ptr<FsckDBDentryTable> dentryTable;
      boost::scoped_ptr<FsckDBFileInodesTable> fileInodesTable;
      boost::scoped_ptr<FsckDBDirInodesTable> dirInodesTable;
      boost::scoped_ptr<FsckDBChunksTable> chunksTable;
      boost::scoped_ptr<FsckDBContDirsTable> contDirsTable;
      boost::scoped_ptr<FsckDBFsIDsTable> fsIDsTable;
      boost::scoped_ptr<FsckDBUsedTargetIDsTable> usedTargetIDsTable;
      boost::scoped_ptr<FsckDBModificationEventsTable> modificationEventsTable;
      DiskList<FsckChunk> malformedChunks;

   public:
      FsckDBDentryTable* getDentryTable()
      {
         return this->dentryTable.get();
      }

      FsckDBFileInodesTable* getFileInodesTable()
      {
         return this->fileInodesTable.get();
      }

      FsckDBDirInodesTable* getDirInodesTable()
      {
         return this->dirInodesTable.get();
      }

      FsckDBChunksTable* getChunksTable()
      {
         return this->chunksTable.get();
      }

      FsckDBContDirsTable* getContDirsTable()
      {
         return this->contDirsTable.get();
      }

      FsckDBFsIDsTable* getFsIDsTable()
      {
         return this->fsIDsTable.get();
      }

      FsckDBUsedTargetIDsTable* getUsedTargetIDsTable()
      {
         return this->usedTargetIDsTable.get();
      }

      FsckDBModificationEventsTable* getModificationEventsTable()
      {
         return this->modificationEventsTable.get();
      }

      DiskList<FsckChunk>* getMalformedChunksList()
      {
         return &malformedChunks;
      }
};

#endif /* FSCKDB_H_ */
