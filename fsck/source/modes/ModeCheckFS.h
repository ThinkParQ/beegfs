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

struct FsckErrCount
{
   FsckErrCount() : unfixableErrors(0), fixableErrors(0)
   {
   }

   FsckErrCount operator+(const FsckErrCount& other)
   {
      unfixableErrors += other.unfixableErrors;
      fixableErrors += other.fixableErrors;
      return *this;
   }

   void operator+=(const FsckErrCount& other)
   {
      *this = *this + other;
   }

   uint64_t getTotalErrors() const
   {
      return unfixableErrors + fixableErrors;
   }

   uint64_t unfixableErrors;
   uint64_t fixableErrors;
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
      FsckErrCount checkAndRepairGeneric(Cursor<Obj> cursor,
         void (ModeCheckFS::*repair)(Obj&, FsckErrCount&, State&), State& state);

      FsckErrCount checkAndRepairDanglingDentry();
      FsckErrCount checkAndRepairWrongInodeOwner();
      FsckErrCount checkAndRepairWrongOwnerInDentry();
      FsckErrCount checkAndRepairOrphanedContDir();
      FsckErrCount checkAndRepairOrphanedDirInode();
      FsckErrCount checkAndRepairOrphanedFileInode();
      FsckErrCount checkAndRepairDuplicateInodes();
      FsckErrCount checkAndRepairOrphanedChunk();
      FsckErrCount checkAndRepairMissingContDir();
      FsckErrCount checkAndRepairWrongFileAttribs();
      FsckErrCount checkAndRepairWrongDirAttribs();
      FsckErrCount checkAndRepairFilesWithMissingTargets();
      FsckErrCount checkAndRepairDirEntriesWithBrokeByIDFile();
      FsckErrCount checkAndRepairOrphanedDentryByIDFiles();
      FsckErrCount checkAndRepairChunksWithWrongPermissions();
      FsckErrCount checkMissingMirrorChunks();
      FsckErrCount checkMissingPrimaryChunks();
      FsckErrCount checkDifferingChunkAttribs();
      FsckErrCount checkAndRepairChunksInWrongPath();
      FsckErrCount checkAndUpdateOldStyledHardlinks();

      void logDuplicateInodeID(checks::DuplicatedInode& dups, int&);

      FsckErrCount checkDuplicateChunks();
      void logDuplicateChunk(std::list<FsckChunk>& dups, FsckErrCount& errCount, int&);

      FsckErrCount checkDuplicateContDirs();
      void logDuplicateContDir(std::list<db::ContDir>& dups, FsckErrCount& errCount, int&);

      FsckErrCount checkMismirroredDentries();
      void logMismirroredDentry(db::DirEntry& entry, FsckErrCount& errCount, int&);

      FsckErrCount checkMismirroredDirectories();
      void logMismirroredDirectory(db::DirInode& dir, FsckErrCount& errCount, int&);

      FsckErrCount checkMismirroredFiles();
      void logMismirroredFile(db::FileInode& file, FsckErrCount& errCount, int&);

      FsckErrCount checkAndRepairMalformedChunk();
      void repairMalformedChunk(FsckChunk& chunk, FsckErrCount& errCount, UserPrompter& prompt);

      void checkAndRepair();

      void repairDanglingDirEntry(db::DirEntry& entry, FsckErrCount& errCount,
         std::pair<UserPrompter*, UserPrompter*>& prompt);
      void repairWrongInodeOwner(FsckDirInode& inode, FsckErrCount& errCount,
         UserPrompter& prompt);
      void repairWrongInodeOwnerInDentry(std::pair<db::DirEntry, NumNodeID>& error,
         FsckErrCount& errCount, UserPrompter& prompt);
      void repairOrphanedDirInode(FsckDirInode& inode, FsckErrCount& errCount,
         UserPrompter& prompt);
      void repairOrphanedFileInode(FsckFileInode& inode, FsckErrCount& errCount,
         UserPrompter& prompt);
      void repairDuplicateInode(checks::DuplicatedInode& dupInode, FsckErrCount& errCount,
         UserPrompter& prompt);
      void repairOrphanedChunk(FsckChunk& chunk, FsckErrCount& errCount, RepairChunkState& state);
      void repairMissingContDir(FsckDirInode& inode, FsckErrCount& errCount, UserPrompter& prompt);
      void repairOrphanedContDir(FsckContDir& dir, FsckErrCount& errCount, UserPrompter& prompt);
      void repairWrongFileAttribs(std::pair<FsckFileInode, checks::OptionalInodeAttribs>& error,
         FsckErrCount& errCount, UserPrompter& prompt);
      void repairWrongDirAttribs(std::pair<FsckDirInode, checks::InodeAttribs>& error,
         FsckErrCount& errCount, UserPrompter& prompt);
      void repairFileWithMissingTargets(db::DirEntry& entry, FsckErrCount& errCount,
         UserPrompter& prompt);
      void repairDirEntryWithBrokenByIDFile(db::DirEntry& entry, FsckErrCount& errCount,
         UserPrompter& prompt);
      void repairOrphanedDentryByIDFile(FsckFsID& id, FsckErrCount& errCount, UserPrompter& prompt);
      void repairChunkWithWrongPermissions(std::pair<FsckChunk, FsckFileInode>& error,
         FsckErrCount& errCount, UserPrompter& prompt);
      void repairWrongChunkPath(std::pair<FsckChunk, FsckFileInode>& error, FsckErrCount& errCount,
         UserPrompter& prompt);
      void updateOldStyledHardlinks(db::FileInode& inode, FsckErrCount& errCount, UserPrompter& prompt);

      void deleteFsIDsFromDB(FsckDirEntryList& dentries);
      void deleteFilesFromDB(FsckDirEntryList& dentries);

      bool ensureLostAndFoundExists();
      void releaseLostAndFound();
};

#endif /* MODECHECKFS_H */
