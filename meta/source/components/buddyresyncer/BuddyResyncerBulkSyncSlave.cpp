#include "BuddyResyncerBulkSyncSlave.h"

#include <common/net/message/storage/mirroring/ResyncRawInodesRespMsg.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/MessagingTk.h>
#include <common/Common.h>

#include <net/message/storage/mirroring/ResyncRawInodesMsgEx.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <program/Program.h>
#include <toolkit/XAttrTk.h>

#include <dirent.h>

BuddyResyncerBulkSyncSlave::BuddyResyncerBulkSyncSlave(BuddyResyncJob& parentJob,
      MetaSyncCandidateStore* syncCandidates, uint8_t slaveID, const NumNodeID& buddyNodeID) :
   SyncSlaveBase("BuddyResyncerBulkSyncSlave_" + StringTk::uintToStr(slaveID), parentJob,
         buddyNodeID),
   syncCandidates(syncCandidates)
{
}

void BuddyResyncerBulkSyncSlave::syncLoop()
{
   EntryLockStore* const lockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   while (!getSelfTerminateNotIdle())
   {
      MetaSyncCandidateDir candidate;
      syncCandidates->fetch(candidate, this);

      // the sync candidate we have retrieved may be invalid if this thread was ordered to
      // terminate and the sync candidate store has no more directories queued for us.
      // in this case, we may end the sync because we have no more candidates, and the resync job
      // guarantees that all gather threads have completed before the bulk syncers are ordered to
      // finish.
      if (syncCandidates->isDirsEmpty() && candidate.getRelativePath().empty() &&
            getSelfTerminate())
         return;

      if (candidate.getType() == MetaSyncDirType::InodesHashDir ||
            candidate.getType() == MetaSyncDirType::DentriesHashDir)
      {
         // lock the hash path in accordance with MkLocalDir, RmLocalDir and RmDir.
         const auto& hashDir = candidate.getRelativePath();
         auto slash1 = hashDir.find('/');
         auto slash2 = hashDir.find('/', slash1 + 1);
         auto hash1 = StringTk::strHexToUInt(hashDir.substr(slash1 + 1, slash2 - slash1 - 1));
         auto hash2 = StringTk::strHexToUInt(hashDir.substr(slash2 + 1));
         HashDirLock hashLock = {lockStore, {hash1, hash2}};

         const FhgfsOpsErr resyncRes = resyncDirectory(candidate, "");
         if (resyncRes == FhgfsOpsErr_SUCCESS)
            continue;

         numDirErrors.increase();
         parentJob->abort(false);
         return;
      }

      // not a hash dir, so it must be a content directory. sync the #fSiDs# first, then the actual
      // content directory. we lock the directory inode the content directory belongs to because we
      // must not allow a concurrent meta action to delete the content directory while we are
      // resyncing it. concurrent modification of directory contents could be allowed, though.

      const std::string dirInodeID = Path(candidate.getRelativePath()).back();
      const std::string fullPath = META_BUDDYMIRROR_SUBDIR_NAME "/" + candidate.getRelativePath();

      FileIDLock dirLock(lockStore, dirInodeID, false);

      // first ensure that the directory still exists - a concurrent modification may have deleted
      // it. this would not be an error; bulk resync should not touch it, an modification sync
      // would remove it completely.
      if (::access(fullPath.c_str(), F_OK) != 0 && errno == ENOENT)
      {
         numDirsSynced.increase(); // Count it anyway, so the sums match up.
         continue;
      }

      MetaSyncCandidateDir fsIDs(
            candidate.getRelativePath() + "/" + META_DIRENTRYID_SUB_STR,
            MetaSyncDirType::InodesHashDir);

      FhgfsOpsErr resyncRes = resyncDirectory(fsIDs, dirInodeID);
      if (resyncRes == FhgfsOpsErr_SUCCESS)
         resyncRes = resyncDirectory(candidate, dirInodeID);

      if (resyncRes != FhgfsOpsErr_SUCCESS)
      {
         numDirErrors.increase();
         parentJob->abort(false);
         return;
      }
      else
      {
         numDirsSynced.increase();
      }
   }
}

FhgfsOpsErr BuddyResyncerBulkSyncSlave::resyncDirectory(const MetaSyncCandidateDir& root,
   const std::string& inodeID)
{
   StreamCandidateArgs args(*this, root, inodeID);

   return resyncAt(Path(root.getRelativePath()), true, streamCandidateDir, &args);
}

FhgfsOpsErr BuddyResyncerBulkSyncSlave::streamCandidateDir(Socket& socket,
   const MetaSyncCandidateDir& candidate, const std::string& inodeID)
{
   EntryLockStore* const lockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   Path candidatePath(META_BUDDYMIRROR_SUBDIR_NAME "/" + candidate.getRelativePath());

   std::unique_ptr<DIR, StorageTk::CloseDirDeleter> dir(opendir(candidatePath.str().c_str()));

   if (!dir)
   {
      LOG(MIRRORING, ERR, "Could not open candidate directory.", candidatePath, sysErr);
      return FhgfsOpsErr_INTERNAL;
   }

   int dirFD = ::dirfd(dir.get());
   if (dirFD < 0)
   {
      LOG(MIRRORING, ERR, "Could not open candidate directory.", candidatePath, sysErr);
      return FhgfsOpsErr_INTERNAL;
   }

   while (true)
   {
      struct dirent* entry;

#if USE_READDIR_P
      struct dirent entryBuf;
      int err = ::readdir_r(dir.get(), &entryBuf, &entry);
#else
      errno = 0;
      entry = readdir(dir.get());
      int err = entry ? 0 : errno;
#endif
      if (err > 0)
      {
         LOG(MIRRORING, ERR, "Could not read candidate directory.", candidatePath, sysErr);
         numDirErrors.increase();
         break;
      }

      if (!entry)
         break;

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
         continue;

      struct stat statData;
      if (::fstatat(dirFD, entry->d_name, &statData, AT_SYMLINK_NOFOLLOW) < 0)
      {
         // the file/directory may have gone away. this is not an error, and the secondary will
         // delete the file/directory as well.
         if (errno == ENOENT)
            continue;

         LOG(MIRRORING, ERR, "Could not stat resync candidate.",
             candidatePath, entry->d_name, sysErr);
         numFileErrors.increase();
         continue;
      }

      if (!S_ISDIR(statData.st_mode) && !S_ISREG(statData.st_mode))
      {
         LOG(MIRRORING, ERR, "Resync candidate is neither file nor directory.",
             candidatePath, entry->d_name, statData.st_mode);
         numFileErrors.increase();
         continue;
      }

      if (candidate.getType() == MetaSyncDirType::ContentDir)
      {
         // if it's in a content directory and a directory, it can really only be the fsids dir.
         // locking for this case is already sorted, so we only have to transfer the (empty)
         // inode metadata to tell the secondary that the directory may stay.
         if (S_ISDIR(statData.st_mode))
         {
            const FhgfsOpsErr streamRes = streamInode(socket, Path(entry->d_name), true);
            if (streamRes != FhgfsOpsErr_SUCCESS)
               return streamRes;
         }
         else
         {
            ParentNameLock dentryLock(lockStore, inodeID, entry->d_name);

            const auto streamRes = streamDentry(socket, Path(), entry->d_name);
            if (streamRes != FhgfsOpsErr_SUCCESS)
            {
               numFileErrors.increase();
               return streamRes;
            }
            else
            {
               numFilesSynced.increase();
            }
         }

         continue;
      }

      // we are now either in a fsids (file inode) directory or a second-level inode hash-dir,
      // which may contain either file or directory inodes. taking a lock unnecessarily is stilll
      // cheaper than reading the inode from disk to determine its type, so just lock the inode id
      // as file
      FileIDLock dirLock(lockStore, entry->d_name, true);

      // access the file once more, because it may have been deleted in the meantime. a new entry
      // with the same name cannot appear in a sane filesystem (that would indicate an ID being
      // reused).
      if (faccessat(dirFD, entry->d_name, F_OK, 0) < 0 && errno == ENOENT)
         continue;

      const FhgfsOpsErr streamRes = streamInode(socket, Path(entry->d_name),
            S_ISDIR(statData.st_mode));
      if (streamRes != FhgfsOpsErr_SUCCESS)
      {
         numFileErrors.increase();
         return streamRes;
      }
      else
      {
         numFilesSynced.increase();
      }
   }

   return sendResyncPacket(socket, std::tuple<>());
}
