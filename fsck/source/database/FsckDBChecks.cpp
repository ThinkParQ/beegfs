#include "FsckDB.h"

#include <database/FsckDBTable.h>
#include <database/Distinct.h>
#include <database/Group.h>
#include <database/LeftJoinEq.h>
#include <database/Select.h>
#include <database/Union.h>
#include <toolkit/DatabaseTk.h>
#include <toolkit/FsckTkEx.h>

#include <program/Program.h>

#include <boost/make_shared.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

namespace {

struct SelectFirstFn
{
   template<typename Left, typename Right>
   Left operator()(std::pair<Left, Right>& pair) const
   {
      return pair.first;
   }
} first;

struct SecondIsNullFn
{
   template<typename Left, typename Right>
   bool operator()(std::pair<Left, Right*>& pair) const
   {
      return pair.second == NULL;
   }
} secondIsNull;

struct ObjectIDFn
{
   template<typename Obj>
   db::EntryID operator()(Obj& obj) const
   {
      return obj.id;
   }
} objectID;

struct FirstObjectIDFn
{
   template<typename Obj, typename Second>
   db::EntryID operator()(std::pair<Obj, Second>& obj) const
   {
      return obj.first.id;
   }
} firstObjectID;

struct SecondIsNotNullFn
{
   template<typename Left, typename Right>
   bool operator()(std::pair<Left, Right*>& pair) const
   {
      return pair.second != NULL;
   }
} secondIsNotNull;

template<typename Left, typename Right, typename Fn>
Filter<LeftJoinEq<Left, Right, Fn>, SecondIsNotNullFn> joinBy(Fn fn, Left left, Right right)
{
   return
      db::leftJoinBy(fn, left, right)
      | db::where(SecondIsNotNullFn() );
}

template<typename To>
struct ConvertTo
{
   template<typename From>
   To operator()(From& from) const
   {
      return To(from);
   }
};

template<typename To>
static db::SelectOp<ConvertTo<To> > convertTo()
{
   return db::select(ConvertTo<To>() );
}

template<typename Inner>
static Cursor<typename Inner::ElementType> cursor(Inner inner)
{
   return Cursor<typename Inner::ElementType>(inner);
}

struct IgnoreByID
{
   SetFragmentCursor<db::ModificationEvent> modified;

   template<typename Source>
   friend Select<
      Filter<
         LeftJoinEq<Source, SetFragmentCursor<db::ModificationEvent>, ObjectIDFn>,
         SecondIsNullFn>,
      SelectFirstFn>
      operator|(Source source, IgnoreByID ignore)
   {
      return
         leftJoinBy(
            objectID,
            source,
            ignore.modified)
         | db::where(secondIsNull)
         | db::select(first);
   }
};

IgnoreByID ignoreByID(SetFragmentCursor<db::ModificationEvent> ids)
{
   IgnoreByID result = { ids };
   return result;
}

struct IDFn
{
   template<typename T>
   T operator()(T t) const { return t; }
} id;

struct StripeSet
{
   std::vector<uint16_t> targets;

   void clear()
   {
      targets.clear();
   }

   void add(db::FileInode& inode)
   {
      if (targets.empty())
      {
         targets.resize(inode.stripePatternSize);
         std::copy(
               inode.targets,
               inode.targets + std::min<unsigned>(inode.NTARGETS, inode.stripePatternSize),
               targets.begin());
      }
   }

   void add(db::StripeTargets& moreTargets)
   {
      unsigned count = std::min<unsigned>(moreTargets.NTARGETS,
            targets.size() - moreTargets.firstTargetIndex);

      std::copy(
            moreTargets.targets,
            moreTargets.targets + count,
            targets.begin() + moreTargets.firstTargetIndex);
   }
};

}



static bool dentryIsDirectory(db::DirEntry& dentry)
{
   return dentry.entryType == FsckDirEntryType_DIRECTORY;
}

static bool dentryIsNotDirectory(db::DirEntry& dentry)
{
   return dentry.entryType != FsckDirEntryType_DIRECTORY;
}

static bool isNotDisposal(db::DirInode& inode)
{
   return !inode.id.isDisposalDir();
}

namespace {
struct DuplicateIDGroup
{
   typedef db::EntryID KeyType;
   typedef db::EntryID ProjType;
   typedef std::set<FsckDuplicateInodeInfo> GroupType;

   bool hasTarget;
   FsckDuplicateInodeInfo lastTarget;
   GroupType group;

   DuplicateIDGroup()
   {
      reset();
   }

   void reset()
   {
      hasTarget = false;
      group.clear();
   }

   KeyType key(std::pair<db::EntryID, FsckDuplicateInodeInfo> pair)
   {
      return pair.first;
   }

   ProjType project(std::pair<db::EntryID, FsckDuplicateInodeInfo> pair)
   {
      return pair.first;
   }

   void step(std::pair<db::EntryID, FsckDuplicateInodeInfo> pair)
   {
      if (!hasTarget)
      {
         lastTarget = pair.second;
         hasTarget = true;
         return;
      }

      if (lastTarget.getSaveNodeID() != 0)
      {
         group.insert(lastTarget);
         lastTarget.setSaveNodeID(0);
      }
      group.insert(pair.second);
   }

   GroupType finish()
   {
      GroupType result;

      result.swap(group);
      reset();
      return result;
   }
};
}

Cursor<checks::DuplicatedInode> FsckDB::findDuplicateInodeIDs()
{
   struct ops
   {
      static std::pair<db::EntryID, FsckDuplicateInodeInfo> idAndTargetF(const db::FileInode& file)
      {
         return std::make_pair(file.id, FsckDuplicateInodeInfo(file.id.str(), file.parentDirID.str(),
                  file.saveNodeID, file.isInlined, file.isBuddyMirrored, DirEntryType_REGULARFILE));
      }

      static std::pair<db::EntryID, FsckDuplicateInodeInfo> idAndTargetD(const db::DirInode& file)
      {
         return std::make_pair(file.id, FsckDuplicateInodeInfo(file.id.str(), file.parentDirID.str(),
                  file.saveNodeID, false, file.isBuddyMirrored, DirEntryType_DIRECTORY));
      }

      static bool hasDuplicateID(const checks::DuplicatedInode& dInode)
      {
         return !dInode.second.empty();
      }
   };

   return cursor(
      db::unionBy(
         id,
         this->dirInodesTable->get()
         | db::where(isNotDisposal)
         | ignoreByID(this->modificationEventsTable->get())
         | db::select(ops::idAndTargetD),
         this->fileInodesTable->getInodes()
         | ignoreByID(this->modificationEventsTable->get())
         | db::select(ops::idAndTargetF) )
      | db::groupBy(DuplicateIDGroup() )
      | db::where(ops::hasDuplicateID) );
}

namespace {
struct DuplicateChunkGroup
{
   typedef std::pair<db::EntryID, uint32_t> KeyType;
   typedef db::EntryID ProjType;
   typedef std::list<FsckChunk> GroupType;

   bool hasChunk;
   db::Chunk chunk;
   GroupType group;

   DuplicateChunkGroup()
   {
      reset();
   }

   void reset()
   {
      hasChunk = false;
      group.clear();
   }

   KeyType key(db::Chunk& chunk)
   {
      return std::make_pair(chunk.id, chunk.targetID);
   }

   ProjType project(db::Chunk& chunk)
   {
      return chunk.id;
   }

   void step(db::Chunk& chunk)
   {
      if(!hasChunk)
      {
         this->chunk = chunk;
         hasChunk = true;
         return;
      }

      if(group.empty() )
         group.push_back(this->chunk);

      group.push_back(chunk);
   }

   GroupType finish()
   {
      GroupType result;

      result.swap(group);
      reset();
      return result;
   }
};
}

Cursor<std::list<FsckChunk> > FsckDB::findDuplicateChunks()
{
   struct ops
   {
      static bool hasDuplicateChunks(std::pair<db::EntryID, std::list<FsckChunk> >& pair)
      {
         return !pair.second.empty();
      }

      static std::list<FsckChunk> second(std::pair<db::EntryID, std::list<FsckChunk> >& pair)
      {
         return pair.second;
      }
   };

   return cursor(
      this->chunksTable->get()
      | ignoreByID(this->modificationEventsTable->get())
      | db::groupBy(DuplicateChunkGroup())
      | db::where(ops::hasDuplicateChunks)
      | db::select(ops::second));
}

namespace {
struct DuplicateContDirGroup
{
   typedef db::EntryID KeyType;
   typedef db::EntryID ProjType;
   typedef std::list<db::ContDir> GroupType;

   bool hasDir;
   db::ContDir lastDir;
   GroupType group;

   DuplicateContDirGroup()
   {
      reset();
   }

   void reset()
   {
      hasDir = false;
      group.clear();
   }

   KeyType key(db::ContDir& dir) { return dir.id; }

   ProjType project(db::ContDir& dir) { return dir.id; }

   void step(db::ContDir& dir)
   {
      if (!hasDir)
      {
         lastDir = dir;
         hasDir = true;
         return;
      }

      if (group.empty())
         group.push_back(lastDir);

      group.push_back(dir);
   }

   GroupType finish()
   {
      GroupType result;

      result.swap(group);
      reset();
      return result;
   }
};
}

Cursor<std::list<db::ContDir>> FsckDB::findDuplicateContDirs()
{
   struct ops
   {
      static bool duplicated(std::pair<db::EntryID, std::list<db::ContDir>>& pair)
      {
         return !pair.second.empty();
      }

      static std::list<db::ContDir> second(std::pair<db::EntryID, std::list<db::ContDir>>& pair)
      {
         return pair.second;
      }

      static bool isNotDisposal(db::ContDir& dir)
      {
         return dir.id != db::EntryID::disposal() && dir.id != db::EntryID::mdisposal();
      }
   };

   return cursor(
      contDirsTable->get()
      | db::where(ops::isNotDisposal)
      | db::groupBy(DuplicateContDirGroup())
      | db::where(ops::duplicated)
      | db::select(ops::second));
}

Cursor<db::DirEntry> FsckDB::findMismirroredDentries()
{
   struct ops
   {
      static bool fileMismirrored(const std::pair<db::DirEntry, db::FileInode*>& row)
      {
         return row.first.isBuddyMirrored != row.second->isBuddyMirrored;
      }

      static bool dirMismirrored(const std::pair<db::DirEntry, db::DirInode*>& row)
      {
         return row.first.isBuddyMirrored != row.second->isBuddyMirrored;
      }
   };

   return cursor(
      db::unionBy(
         objectID,
         db::leftJoinBy(
            objectID,
            dentryTable->get()
            | ignoreByID(modificationEventsTable->get())
            | db::where(dentryIsNotDirectory),
            fileInodesTable->getInodes())
         | db::where(secondIsNotNull)
         | db::where(ops::fileMismirrored)
         | db::select(first),
         db::leftJoinBy(
            objectID,
            dentryTable->get()
            | ignoreByID(modificationEventsTable->get())
            | db::where(dentryIsDirectory),
            dirInodesTable->get())
         | db::where(secondIsNotNull)
         | db::where(ops::dirMismirrored)
         | db::select(first)));
}

Cursor<db::FileInode> FsckDB::findMismirroredFiles()
{
   struct ops
   {
      static bool mismirrored(db::FileInode& file)
      {
         return file.isMismirrored;
      }
   };

   return cursor(
      fileInodesTable->getInodes()
      | ignoreByID(modificationEventsTable->get())
      | db::where(ops::mismirrored));
}

Cursor<db::DirInode> FsckDB::findMismirroredDirectories()
{
   struct ops
   {
      static bool dirMismirrored(const std::pair<db::DirInode, db::ContDir*>& row)
      {
         return row.first.isMismirrored || row.first.isBuddyMirrored != row.second->isBuddyMirrored;
      }
   };

   return cursor(
      db::leftJoinBy(
         objectID,
         dirInodesTable->get()
         | ignoreByID(modificationEventsTable->get()),
         contDirsTable->get())
      | db::where(secondIsNotNull)
      | db::where(ops::dirMismirrored)
      | db::select(first));
}

/*
 * looks for dir entries, for which no inode was found (dangling dentries) and directly inserts
 * them into the database
 */
Cursor<db::DirEntry> FsckDB::findDanglingDirEntries()
{
   return cursor(
      db::unionBy(
         objectID,
         db::leftJoinBy(
            objectID,
            this->dentryTable->get()
            | ignoreByID(this->modificationEventsTable->get() )
            | db::where(dentryIsNotDirectory),
            this->fileInodesTable->getInodes() )
         | db::where(secondIsNull)
         | db::select(first),
         db::leftJoinBy(
            objectID,
            this->dentryTable->get()
            | ignoreByID(this->modificationEventsTable->get() )
            | db::where(dentryIsDirectory),
            this->dirInodesTable->get() )
         | db::where(secondIsNull)
         | db::select(first) )
      | db::distinctBy(objectID) );
}

/*
 * looks for dir entries, for which the inode was found on a different host as expected (according
 * to inodeOwner info in dentry)
 */
Cursor<std::pair<db::DirEntry, NumNodeID> > FsckDB::findDirEntriesWithWrongOwner()
{
   struct ops
   {
      static bool fileInodeOwnerIncorrect(std::pair<db::DirEntry, db::FileInode*>& pair)
      {
         return pair.first.inodeOwnerNodeID != pair.second->saveNodeID;
      }

      static bool dirInodeOwnerIncorrect(std::pair<db::DirEntry, db::DirInode*>& pair)
      {
         return pair.first.inodeOwnerNodeID != pair.second->saveNodeID;
      }

      static std::pair<db::DirEntry, NumNodeID> resultF(
         std::pair<db::DirEntry, db::FileInode*>& pair)
      {
         return std::make_pair(pair.first, NumNodeID(pair.second->saveNodeID) );
      }

      static std::pair<db::DirEntry, NumNodeID> resultD(
         std::pair<db::DirEntry, db::DirInode*>& pair)
      {
         return std::make_pair(pair.first, NumNodeID(pair.second->saveNodeID) );
      }
   };

   return cursor(
      db::unionBy(
         firstObjectID,
         joinBy(
            objectID,
            this->dentryTable->get()
            | ignoreByID(this->modificationEventsTable->get() )
            | db::where(dentryIsNotDirectory),
            this->fileInodesTable->getInodes() )
         | db::where(ops::fileInodeOwnerIncorrect)
         | db::select(ops::resultF),
         joinBy(
            objectID,
            this->dentryTable->get()
            | ignoreByID(this->modificationEventsTable->get() )
            | db::where(dentryIsDirectory),
            this->dirInodesTable->get() )
         | db::where(ops::dirInodeOwnerIncorrect)
         | db::select(ops::resultD) )
      | db::distinctBy(firstObjectID) );
}

/*
 * looks for dir inodes, for which the owner node information is not correct and directly inserts
 * them into the database
 */
Cursor<FsckDirInode> FsckDB::findInodesWithWrongOwner()
{
   struct ops
   {
      static bool inodeHasWrongOwner(db::DirInode& inode)
      {
         return inode.ownerNodeID != inode.saveNodeID
            && !inode.id.isDisposalDir();
      }
   };

   return Cursor<FsckDirInode>(
      this->dirInodesTable->get()
      | ignoreByID(this->modificationEventsTable->get() )
      | db::where(ops::inodeHasWrongOwner)
      | convertTo<FsckDirInode>() );
}

namespace {
struct JoinDirEntriesWithBrokenByIDFile
{
   typedef boost::tuple<
         db::EntryID, // ID
         db::EntryID, // parentDirID
         int,         // device
         NumNodeID,   // saveNodeID
         uint64_t     // saveInode
      > result_type;

   template<typename Obj>
   result_type operator()(Obj& obj) const
   {
      return result_type(obj.id, obj.parentDirID, obj.saveDevice, NumNodeID(obj.saveNodeID),
         obj.saveInode);
   }
};
}

/*
 * looks for dir entries, for which no corresponding dentry-by-id file was found in #fsids# and
 * directly inserts them into the database (only dir entries with inlined inodes are relevant)
 */
Cursor<db::DirEntry> FsckDB::findDirEntriesWithBrokenByIDFile()
{
   struct ops
   {
      static bool fsidFileMissing(std::pair<db::DirEntry, db::FsID*>& pair)
      {
         return pair.first.hasInlinedInode
            && !pair.first.parentDirID.isDisposalDir()
            && pair.second == NULL;
      }
   };

   return cursor(
      db::leftJoinBy(
         JoinDirEntriesWithBrokenByIDFile(),
         this->dentryTable->get()
         | ignoreByID(this->modificationEventsTable->get() ),
         this->fsIDsTable->get() )
      | db::where(ops::fsidFileMissing)
      | db::select(first) );
}

namespace {
struct JoinOrphanedFsIDs
{
   typedef boost::tuple<
         db::EntryID, // ID
         db::EntryID, // parentDirID
         NumNodeID    // saveNodeID
      > result_type;

   template<typename Obj>
   result_type operator()(Obj& obj) const
   {
      return result_type(obj.id, obj.parentDirID, NumNodeID(obj.saveNodeID) );
   }
};
}

/*
 * looks for dentry-by-ID files in #fsids#, for which no corresponding dentry file was found and
 * directly inserts them into the database
 */
Cursor<FsckFsID> FsckDB::findOrphanedFsIDFiles()
{
   return Cursor<FsckFsID>(
      db::leftJoinBy(
         JoinOrphanedFsIDs(),
         this->fsIDsTable->get()
         | ignoreByID(this->modificationEventsTable->get() ),
         this->dentryTable->get() )
      | db::where(secondIsNull)
      | db::select(first)
      | convertTo<FsckFsID>() );
}

/*
 * looks for dir inodes, for which no dir entry exists
 */
Cursor<FsckDirInode> FsckDB::findOrphanedDirInodes()
{
   struct ops
   {
      static bool inodeHasNoDentry(std::pair<db::DirInode, db::DirEntry*>& pair)
      {
         return pair.first.id != db::EntryID::root()
            && !pair.first.id.isDisposalDir()
            && pair.second == NULL;
      }
   };

   return Cursor<FsckDirInode>(
      db::leftJoinBy(
         objectID,
         this->dirInodesTable->get()
         | ignoreByID(this->modificationEventsTable->get() ),
         this->dentryTable->get() )
      | db::where(ops::inodeHasNoDentry)
      | db::select(first)
      | convertTo<FsckDirInode>() );
}

/*
 * looks for file inodes, for which no dir entry exists
 */
Cursor<FsckFileInode> FsckDB::findOrphanedFileInodes()
{
   struct ops
   {
      static FsckFileInode fileInodeToFsckFileInode(const db::FileInode& inode)
      {
         return inode.toInodeWithoutStripes();
      }
   };

   return Cursor<FsckFileInode>(
      db::leftJoinBy(
         objectID,
         this->fileInodesTable->getInodes()
         | ignoreByID(this->modificationEventsTable->get() ),
         this->dentryTable->get() )
      | db::where(secondIsNull)
      | db::select(first)
      | db::select(ops::fileInodeToFsckFileInode) );
}

namespace {
struct StripePatternGroup
{
   typedef db::EntryID KeyType;
   typedef db::FileInode ProjType;
   typedef std::vector<uint16_t> GroupType;

   StripeSet pattern;

   KeyType key(std::pair<db::FileInode, db::StripeTargets*>& pair)
   {
      return pair.first.id;
   }

   ProjType project(std::pair<db::FileInode, db::StripeTargets*>& pair)
   {
      return pair.first;
   }

   void step(std::pair<db::FileInode, db::StripeTargets*>& pair)
   {
      pattern.add(pair.first);
      if (pair.second)
         pattern.add(*pair.second);
   }

   GroupType finish()
   {
      std::vector<uint16_t> result = pattern.targets;

      pattern.clear();

      std::sort(result.begin(), result.end());
      return result;
   }
};

struct OrphanedChunksJoin
{
   db::EntryID operator()(db::Chunk& chunk) const { return chunk.id; }
   db::EntryID operator()(std::pair<db::FileInode, std::vector<uint16_t>>& pair)
   {
      return pair.first.id;
   }
};
}

/*
 * looks for chunks, for which no inode exists
 */
Cursor<FsckChunk> FsckDB::findOrphanedChunks()
{
   struct ops
   {
      static bool isOrphaned(
         std::pair<db::Chunk, std::pair<db::FileInode, std::vector<uint16_t>>*>& pair)
      {
         if (!pair.second)
            return true;

         const db::Chunk& chunk = pair.first;
         const db::FileInode& inode = pair.second->first;
         const std::vector<uint16_t>& targets = pair.second->second;

         if (inode.stripePatternType == FsckStripePatternType_BUDDYMIRROR)
            return !std::binary_search(targets.begin(), targets.end(), chunk.buddyGroupID);
         else
            return !std::binary_search(targets.begin(), targets.end(), chunk.targetID);
      }
   };

   return Cursor<FsckChunk>(
      db::leftJoinBy(
         OrphanedChunksJoin(),
         this->chunksTable->get()
         | ignoreByID(this->modificationEventsTable->get() ),
         db::leftJoinBy(
            objectID,
            fileInodesTable->getInodes(),
            fileInodesTable->getTargets())
         | db::groupBy(StripePatternGroup()))
      | db::where(ops::isOrphaned)
      | db::select(first)
      | convertTo<FsckChunk>() );
}

/*
 * looks for dir inodes, for which no .cont directory exists
 */
Cursor<FsckDirInode> FsckDB::findInodesWithoutContDir()
{
   return Cursor<FsckDirInode>(
      db::leftJoinBy(
         objectID,
         this->dirInodesTable->get()
         | ignoreByID(this->modificationEventsTable->get() ),
         this->contDirsTable->get() )
      | db::where(secondIsNull)
      | db::select(first)
      | convertTo<FsckDirInode>() );
}

/*
 * looks for content directories, for which no inode exists
 */
Cursor<FsckContDir> FsckDB::findOrphanedContDirs()
{
   return Cursor<FsckContDir>(
      db::leftJoinBy(
         objectID,
         this->contDirsTable->get()
         | ignoreByID(this->modificationEventsTable->get() ),
         this->dirInodesTable->get() )
      | db::where(secondIsNull)
      | db::select(first)
      | convertTo<FsckContDir>() );
}

namespace {
struct FindWrongInodeFileAttribsGrouperDentry
{
   typedef db::EntryID KeyType;
   typedef db::FileInode ProjType;
   typedef uint64_t GroupType;

   GroupType count;

   FindWrongInodeFileAttribsGrouperDentry()
      : count(0)
   {}

   KeyType key(std::pair<db::FileInode, db::DirEntry*>& pair)
   {
      return pair.first.id;
   }

   ProjType project(std::pair<db::FileInode, db::DirEntry*>& pair)
   {
      return pair.first;
   }

   void step(std::pair<db::FileInode, db::DirEntry*>& pair)
   {
      this->count++;
   }

   GroupType finish()
   {
      uint64_t result = this->count;
      this->count = 0;
      return result;
   }
};

struct FindWrongInodeFileAttribsGrouperChunks
{
   typedef db::EntryID KeyType;
   typedef db::FileInode ProjType;
   typedef uint64_t GroupType;

   int64_t chunkSizeSum;
   int64_t expectedFileSize;
   StripeSet pattern;
   std::map<uint16_t, FsckChunk> fileChunks;
   unsigned chunkSize;

   FindWrongInodeFileAttribsGrouperChunks()
   {
      reset();
   }

   void reset()
   {
      this->chunkSizeSum = 0;
      this->expectedFileSize = 0;
      this->pattern.clear();
      this->fileChunks.clear();
      this->chunkSize = 0;
   }

   KeyType key(std::pair<std::pair<db::FileInode, db::StripeTargets*>, db::Chunk*>& pair)
   {
      return pair.first.first.id;
   }

   ProjType project(std::pair<std::pair<db::FileInode, db::StripeTargets*>, db::Chunk*>& pair)
   {
      return pair.first.first;
   }

   void step(std::pair<std::pair<db::FileInode, db::StripeTargets*>, db::Chunk*>& pair)
   {
      db::FileInode& inode = pair.first.first;
      db::StripeTargets* targets = pair.first.second;

      if(pair.second == NULL)
      {
         this->chunkSizeSum = 0;
         this->expectedFileSize = 1;
         return;
      }

      this->chunkSizeSum += pair.second->fileSize;

      this->fileChunks[pair.second->targetID] = *pair.second;

      if(!this->chunkSize)
      {
         this->chunkSize = inode.chunkSize;
         this->expectedFileSize = inode.fileSize;

         pattern.add(inode);
      }

      if(targets != NULL)
         pattern.add(*targets);
   }

   GroupType finish()
   {
      // chunk file sizes match expected size from inode, don't have to calculate actual file size
      // from chunk sizes
      if(this->chunkSizeSum == this->expectedFileSize)
      {
         uint64_t result = this->chunkSizeSum;
         reset();
         return result;
      }

      // otherwise perform size check
      ChunkFileInfoVec chunkFileInfoVec;

      for (auto it = pattern.targets.begin(); it != pattern.targets.end(); it++)
      {
         std::map<uint16_t, FsckChunk>::const_iterator chunk = this->fileChunks.find(*it);

         int64_t fileSize = 0;
         uint64_t usedBlocks = 0;

         if(chunk != this->fileChunks.end() )
         {
            fileSize = chunk->second.getFileSize();
            usedBlocks = chunk->second.getUsedBlocks();
         }

         DynamicFileAttribs fileAttribs(1, fileSize, usedBlocks, 0, 0);
         ChunkFileInfo stripeNodeFileInfo(this->chunkSize, MathTk::log2Int32(this->chunkSize),
            fileAttribs);
         chunkFileInfoVec.push_back(stripeNodeFileInfo);
      }

      // now perform the actual file size check; this check is still pretty expensive and
      // should be optimized

      // this might be a buddy mirror pattern, but we are not interested in mirror targets
      // here, so we just create it as RAID0 pattern
      StripePattern* stripePattern = FsckTk::FsckStripePatternToStripePattern(
         FsckStripePatternType_RAID0, this->chunkSize, &pattern.targets);
      StatData statData;

      statData.setAllFake();

      statData.updateDynamicFileAttribs(chunkFileInfoVec, stripePattern);

      SAFE_DELETE(stripePattern);

      reset();
      return statData.getFileSize();
   }
};

// 
struct FindWrongInodeFileAttribsGrouper
{
   typedef db::EntryID KeyType;
   typedef FsckFileInode ProjType;
   typedef checks::OptionalInodeAttribs GroupType;

   GroupType state;

   FindWrongInodeFileAttribsGrouper()
   {
      reset();
   }

   void reset()
   {
      state.reset();
   }

   KeyType key(std::pair<db::FileInode, checks::OptionalInodeAttribs>& pair)
   {
      return pair.first.id;
   }

   ProjType project(std::pair<db::FileInode, checks::OptionalInodeAttribs>& pair)
   {
      return pair.first.toInodeWithoutStripes();
   }

   void step(std::pair<db::FileInode, checks::OptionalInodeAttribs>& pair)
   {
      if (pair.second.size) {
         if (state.size)
            *state.size += *pair.second.size;
         else
            state.size = pair.second.size;
      }

      if (pair.second.nlinks) {
         if (state.nlinks)
            *state.nlinks += *pair.second.nlinks;
         else
            state.nlinks = pair.second.nlinks;
      }
   }

   GroupType finish()
   {
      GroupType result = state;
      reset();
      return result;
   }
};

struct JoinWithStripedInode
{
   typedef db::EntryID result_type;

   db::EntryID operator()(const db::DirEntry& d) const { return d.id; }
   db::EntryID operator()(const db::Chunk& c) const { return c.id; }

   db::EntryID operator()(const std::pair<db::FileInode, db::StripeTargets*>& p) const
   {
      return p.first.id;
   }
};
}

/*
 * looks for file inodes, for which the saved attribs (e.g. filesize) are not
 * equivalent to those of the primary chunks
 */
Cursor<std::pair<FsckFileInode, checks::OptionalInodeAttribs> > FsckDB::findWrongInodeFileAttribs()
{
   // Note: ignore dentries in disposal as they are not counted in numHardlinks in the inodes

   struct ops
   {
      static bool fileAttribsIncorrect(std::pair<db::FileInode, uint64_t>& pair)
      {
         return pair.first.fileSize != pair.second;
      }

      static std::pair<db::FileInode, checks::OptionalInodeAttribs> fileAttribs(
         std::pair<db::FileInode, uint64_t>& pair)
      {
         checks::OptionalInodeAttribs attribs = { pair.second, {} };
         return std::make_pair(pair.first, attribs);
      }

      static bool linkCountIncorrect(std::pair<db::FileInode, uint64_t>& pair)
      {
         return pair.first.numHardlinks != pair.second;
      }

      static std::pair<db::FileInode, checks::OptionalInodeAttribs> linkCount(
         std::pair<db::FileInode, uint64_t>& pair)
      {
         checks::OptionalInodeAttribs attribs = { {}, pair.second };
         return std::make_pair(pair.first, attribs);
      }

      static bool dentryNotInDisposal(db::DirEntry& dentry)
      {
         return !dentry.parentDirID.isDisposalDir();
      }
   };

   return cursor(
      db::unionBy(
         firstObjectID,
         db::leftJoinBy(
            JoinWithStripedInode(),
            db::leftJoinBy(
               objectID,
               this->fileInodesTable->getInodes()
               | ignoreByID(this->modificationEventsTable->get() ),
               this->fileInodesTable->getTargets() ),
            this->chunksTable->get() )
         | db::groupBy(FindWrongInodeFileAttribsGrouperChunks() )
         | db::where(ops::fileAttribsIncorrect)
         | db::select(ops::fileAttribs),
         db::leftJoinBy(
            objectID,
            this->fileInodesTable->getInodes()
            | ignoreByID(this->modificationEventsTable->get() ),
            this->dentryTable->get()
            | db::where(ops::dentryNotInDisposal) )
         | db::groupBy(FindWrongInodeFileAttribsGrouperDentry() )
         | db::where(ops::linkCountIncorrect)
         | db::select(ops::linkCount) )
      | db::groupBy(FindWrongInodeFileAttribsGrouper() ) );
}

namespace {
struct WrongDirInodeGrouper
{
   typedef std::pair<db::DirInode, FsckDBDentryTable::ByParent*> InRowType;

   typedef db::EntryID KeyType;
   typedef db::DirInode ProjType;
   typedef checks::InodeAttribs GroupType;

   GroupType state;

   WrongDirInodeGrouper()
   {
      memset(&state, 0, sizeof(state) );
   }

   KeyType key(InRowType& row)
   {
      return row.first.id;
   }

   ProjType project(InRowType& row)
   {
      return row.first;
   }

   void step(InRowType& row)
   {
      if(row.second == NULL)
         return;

      this->state.size += 1;

      if(row.second->entryType == FsckDirEntryType_DIRECTORY)
         this->state.nlinks += 1;
   }

   GroupType finish()
   {
      GroupType result = this->state;
      result.nlinks += 2; // for . and ..
      memset(&state, 0, sizeof(state) );
      return result;
   }
};

struct JoinWrongInodeDirAttribs
{
   typedef db::EntryID result_type;

   db::EntryID operator()(db::DirInode& inode) const { return inode.id; }
   db::EntryID operator()(FsckDBDentryTable::ByParent& dentry) const { return dentry.parent; }
};
}

/*
 * looks for dir inodes, for which the saved attribs (e.g. size) are not correct
 */
Cursor<std::pair<FsckDirInode, checks::InodeAttribs> > FsckDB::findWrongInodeDirAttribs()
{
   struct ops
   {
      static bool dirAttributesIncorrect(std::pair<db::DirInode, checks::InodeAttribs>& row)
      {
         db::DirInode& inode = row.first;
         checks::InodeAttribs data = row.second;

         return inode.size != data.size || inode.numHardlinks != data.nlinks;
      }
   };

   return cursor(
      db::leftJoinBy(
         JoinWrongInodeDirAttribs(),
         this->dirInodesTable->get()
         | ignoreByID(this->modificationEventsTable->get() )
         | db::where(isNotDisposal),
         this->dentryTable->getByParent() )
      | db::groupBy(WrongDirInodeGrouper() )
      | db::where(ops::dirAttributesIncorrect)
      | convertTo<std::pair<FsckDirInode, checks::InodeAttribs> >() );
}

namespace {
struct HasMissingTarget
{
   const std::set<uint16_t> missingTargets;
   const std::set<uint16_t> missingBuddyGroups;

   HasMissingTarget(const std::set<uint16_t>& missingTargets,
         const std::set<uint16_t>& missingBuddyGroups)
      : missingTargets(missingTargets), missingBuddyGroups(missingBuddyGroups)
   {}

   bool operator()(std::pair<db::DirEntry, std::pair<db::FileInode, db::StripeTargets*>*>& pair)
   {
      db::FileInode& inode = pair.second->first;
      db::StripeTargets* targets = pair.second->second;

      const std::set<uint16_t>* missingTargets = NULL;

      switch(inode.stripePatternType)
      {
      case FsckStripePatternType_RAID0:
         missingTargets = &this->missingTargets;
         break;

      case FsckStripePatternType_BUDDYMIRROR:
         missingTargets = &this->missingBuddyGroups;
         break;

      default:
         return false;
      }

      unsigned inodeStripes = std::min<unsigned>(inode.stripePatternSize, inode.NTARGETS);
      for(unsigned i = 0; i < inodeStripes; i++)
      {
         if(missingTargets->count(inode.targets[i]) )
            return true;
      }

      if(targets)
      {
         unsigned limit = std::min<unsigned>(targets->NTARGETS,
            inode.stripePatternSize - targets->firstTargetIndex);

         for(unsigned i = 0; i < limit; i++)
         {
            if(missingTargets->count(targets->targets[i]) )
               return true;
         }
      }

      return false;
   }
};
}

/*
 * looks for file inodes, which have a stripe target set, which does not exist
 */
Cursor<db::DirEntry> FsckDB::findFilesWithMissingStripeTargets(TargetMapper* targetMapper,
   MirrorBuddyGroupMapper* buddyGroupMapper)
{
   FsckTargetIDList usedTargets;
   std::set<uint16_t> missingTargets;
   std::set<uint16_t> missingBuddyGroups;

   {
      SetFragmentCursor<db::UsedTarget> c = this->usedTargetIDsTable->get();
      while(c.step() )
         usedTargets.push_back(*c.get() );
   }

   for(FsckTargetIDListIter it = usedTargets.begin(); it != usedTargets.end(); ++it)
   {
      switch(it->getTargetIDType() )
      {
      case FsckTargetIDType_TARGET:
         if(!targetMapper->targetExists(it->getID() ) )
            missingTargets.insert(it->getID() );
         break;

      case FsckTargetIDType_BUDDYGROUP: {
         MirrorBuddyGroup group = buddyGroupMapper->getMirrorBuddyGroup(it->getID() );
         if(group.firstTargetID == 0 && group.secondTargetID == 0)
            missingBuddyGroups.insert(it->getID() );

         break;
      }
      }
   }

   return cursor(
      joinBy(
         JoinWithStripedInode(),
         this->dentryTable->get(),
         db::leftJoinBy(
            objectID,
            this->fileInodesTable->getInodes(),
            this->fileInodesTable->getTargets() ) )
      | db::where(HasMissingTarget(missingTargets, missingBuddyGroups) )
      | db::select(first)
      | db::distinctBy(objectID) );
}

/*
 * looks for chunks, which have wrong owner/group
 *
 * important for quotas
 */
Cursor<std::pair<FsckChunk, FsckFileInode> > FsckDB::findChunksWithWrongPermissions()
{
   struct ops
   {
      static bool chunkHasWrongPermissions(std::pair<db::Chunk, db::FileInode*>& pair)
      {
         return pair.first.uid != pair.second->uid
            || pair.first.gid != pair.second->gid;
      }

      static std::pair<FsckChunk, FsckFileInode> result(std::pair<db::Chunk, db::FileInode*>& pair)
      {
         return std::make_pair(pair.first, pair.second->toInodeWithoutStripes() );
      }
   };

   return cursor(
      joinBy(
         objectID,
         this->chunksTable->get()
         | ignoreByID(this->modificationEventsTable->get() ),
         this->fileInodesTable->getInodes() )
      | db::where(ops::chunkHasWrongPermissions)
      | db::select(ops::result) );
}

/*
 * looks for chunks, which are saved in the wrong place
 */
Cursor<std::pair<FsckChunk, FsckFileInode> > FsckDB::findChunksInWrongPath()
{
   struct ops
   {
      static bool chunkInWrongPath(std::pair<db::Chunk, db::FileInode*>& pair)
      {
         db::Chunk& chunk = pair.first;
         db::FileInode& file = *pair.second;

         return chunk.savedPath !=
            DatabaseTk::calculateExpectedChunkPath(chunk.id.str(), file.origParentUID,
               file.origParentEntryID.str(), file.pathInfoFlags);
      }

      static std::pair<FsckChunk, FsckFileInode> result(std::pair<db::Chunk, db::FileInode*>& pair)
      {
         return std::make_pair(pair.first, pair.second->toInodeWithoutStripes() );
      }
   };

   return cursor(
      joinBy(
         objectID,
         this->chunksTable->get()
         | ignoreByID(this->modificationEventsTable->get() ),
         this->fileInodesTable->getInodes() )
      | db::where(ops::chunkInWrongPath)
      | db::select(ops::result) );
}

/**
 * Find file inode(s) which are inlined and have linkCount > 1 (i.e. old styled links)
 */
Cursor<db::FileInode> FsckDB::findFilesWithMultipleHardlinks()
{
   struct ops
   {
      static bool hasMultipleHardlinks(db::FileInode& inode)
      {
         return (inode.numHardlinks > 1 && inode.isInlined);
      }
   };

   return cursor(
      fileInodesTable->getInodes()
      | ignoreByID(modificationEventsTable->get())
      | db::where(ops::hasMultipleHardlinks));
}
