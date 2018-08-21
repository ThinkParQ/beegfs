#ifndef MODECHECKFS_H
#define MODECHECKFS_H

#include <boost/scoped_ptr.hpp>
#include <vector>

#include <database/FsckDB.h>
#include <database/FsckDBException.h>
#include <modes/Mode.h>

class UserPrompter
{
   public:
      template<unsigned Actions>
      UserPrompter(const FsckRepairAction (&possibleActions)[Actions],
         FsckRepairAction defaultRepairAction);

      FsckRepairAction chooseAction(const std::string& prompt);

   private:
      bool askForAction;
      std::vector<FsckRepairAction> possibleActions;
      FsckRepairAction repairAction;
};

struct RepairChunkState
{
   UserPrompter* prompt;
   std::string lastID;
   FsckRepairAction lastChunkAction;
};

class ModeCheckFS : public Mode
{
   public:
      ModeCheckFS();
      virtual ~ModeCheckFS();

      static void printHelp();

      virtual int execute();

   private:
      boost::scoped_ptr<FsckDB> database;

      LogContext log;

      NodeHandle lostAndFoundNode;
      EntryInfo lostAndFoundInfo;
      boost::shared_ptr<FsckDirInode> lostAndFoundInode;

      std::set<NumNodeID> secondariesSetBad;

      int initDatabase();
      void printHeaderInformation();
      void disposeUnusedFiles();
      FhgfsOpsErr gatherData(bool forceRestart);

      template<typename Obj, typename State>
      int64_t checkAndRepairGeneric(Cursor<Obj> cursor,
         void (ModeCheckFS::*repair)(Obj&, State&), State& state);

      int64_t checkAndRepairDanglingDentry();
      int64_t checkAndRepairWrongInodeOwner();
      int64_t checkAndRepairWrongOwnerInDentry();
      int64_t checkAndRepairOrphanedContDir();
      int64_t checkAndRepairOrphanedDirInode();
      int64_t checkAndRepairOrphanedFileInode();
      int64_t checkAndRepairOrphanedChunk();
      int64_t checkAndRepairMissingContDir();
      int64_t checkAndRepairWrongFileAttribs();
      int64_t checkAndRepairWrongDirAttribs();
      int64_t checkAndRepairFilesWithMissingTargets();
      int64_t checkAndRepairDirEntriesWithBrokeByIDFile();
      int64_t checkAndRepairOrphanedDentryByIDFiles();
      int64_t checkAndRepairChunksWithWrongPermissions();
      int64_t checkMissingMirrorChunks();
      int64_t checkMissingPrimaryChunks();
      int64_t checkDifferingChunkAttribs();
      int64_t checkAndRepairChunksInWrongPath();

      int64_t checkDuplicateInodeIDs();
      void logDuplicateInodeID(checks::DuplicatedInode& dups, int&);

      int64_t checkDuplicateChunks();
      void logDuplicateChunk(std::list<FsckChunk>& dups, int&);

      int64_t checkDuplicateContDirs();
      void logDuplicateContDir(std::list<db::ContDir>& dups, int&);

      int64_t checkMismirroredDentries();
      void logMismirroredDentry(db::DirEntry& entry, int&);

      int64_t checkMismirroredDirectories();
      void logMismirroredDirectory(db::DirInode& dir, int&);

      int64_t checkMismirroredFiles();
      void logMismirroredFile(db::FileInode& file, int&);

      int64_t checkAndRepairMalformedChunk();
      void repairMalformedChunk(FsckChunk& chunk, UserPrompter& prompt);

      void checkAndRepair();

      void repairDanglingDirEntry(db::DirEntry& entry,
         std::pair<UserPrompter*, UserPrompter*>& prompt);
      void repairWrongInodeOwner(FsckDirInode& inode, UserPrompter& prompt);
      void repairWrongInodeOwnerInDentry(std::pair<db::DirEntry, NumNodeID>& error,
         UserPrompter& prompt);
      void repairOrphanedDirInode(FsckDirInode& inode, UserPrompter& prompt);
      void repairOrphanedFileInode(FsckFileInode& inode, UserPrompter& prompt);
      void repairOrphanedChunk(FsckChunk& chunk, RepairChunkState& state);
      void repairMissingContDir(FsckDirInode& inode, UserPrompter& prompt);
      void repairOrphanedContDir(FsckContDir& dir, UserPrompter& prompt);
      void repairWrongFileAttribs(std::pair<FsckFileInode, checks::InodeAttribs>& error,
         UserPrompter& prompt);
      void repairWrongDirAttribs(std::pair<FsckDirInode, checks::InodeAttribs>& error,
         UserPrompter& prompt);
      void repairFileWithMissingTargets(db::DirEntry& entry, UserPrompter& prompt);
      void repairDirEntryWithBrokenByIDFile(db::DirEntry& entry, UserPrompter& prompt);
      void repairOrphanedDentryByIDFile(FsckFsID& id, UserPrompter& prompt);
      void repairChunkWithWrongPermissions(std::pair<FsckChunk, FsckFileInode>& error,
         UserPrompter& prompt);
      void repairWrongChunkPath(std::pair<FsckChunk, FsckFileInode>& error, UserPrompter& prompt);

      void deleteFsIDsFromDB(FsckDirEntryList& dentries);
      void deleteFilesFromDB(FsckDirEntryList& dentries);

      bool ensureLostAndFoundExists();
      void releaseLostAndFound();
};

#endif /* MODECHECKFS_H */
