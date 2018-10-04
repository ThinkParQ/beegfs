#include <common/storage/Metadata.h>
#include <common/app/log/LogContext.h>
#include <common/toolkit/StringTk.h>
#include <toolkit/BuddyCommTk.h>
#include <program/Program.h>

#include "BuddyResyncerGatherSlave.h"

BuddyResyncerGatherSlave::BuddyResyncerGatherSlave(MetaSyncCandidateStore* syncCandidates) :
   PThread("BuddyResyncerGatherSlave"),
   isRunning(false),
   syncCandidates(syncCandidates)
{
   metaBuddyPath = Program::getApp()->getMetaPath() + "/" CONFIG_BUDDYMIRROR_SUBDIR_NAME;
}

void BuddyResyncerGatherSlave::run()
{
   setIsRunning(true);

   try
   {
      LOG(MIRRORING, DEBUG, "Component started");
      registerSignalHandler();
      workLoop();
      LOG(MIRRORING, DEBUG, "Component stopped");
   }
   catch (std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

   setIsRunning(false);
}

void BuddyResyncerGatherSlave::workLoop()
{
   crawlDir(metaBuddyPath + "/" META_INODES_SUBDIR_NAME, MetaSyncDirType::InodesHashDir);
   crawlDir(metaBuddyPath + "/" META_DENTRIES_SUBDIR_NAME, MetaSyncDirType::DentriesHashDir);
}

void BuddyResyncerGatherSlave::crawlDir(const std::string& path, const MetaSyncDirType type,
   const unsigned level)
{
   LOG_DBG(MIRRORING, DEBUG, "Entering hash dir.", level, path);

   std::unique_ptr<DIR, StorageTk::CloseDirDeleter> dirHandle(::opendir(path.c_str()));

   if (!dirHandle)
   {
      LOG(MIRRORING, ERR, "Unable to open path", path, sysErr);
      numErrors.increase();
      return;
   }

   while (!getSelfTerminate())
   {
      struct dirent* entry;

#if USE_READDIR_R
      struct dirent buffer;
      const int readRes = ::readdir_r(dirHandle.get(), &buffer, &entry);
#else
      errno = 0;
      entry = ::readdir(dirHandle.get());
      const int readRes = entry ? 0 : errno;
#endif
      if (readRes != 0)
      {
         LOG(MIRRORING, ERR, "Could not read dir entry.", path, sysErr(readRes));
         numErrors.increase();
         return;
      }

      if (!entry)
         break;

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
         continue;

      const std::string& candidatePath = path + "/" + entry->d_name;

      struct stat statBuf;
      const int statRes = ::stat(candidatePath.c_str(), &statBuf);
      if (statRes)
      {
         // in a 2nd level dentry hashdir, content directories may disappear - this is not an error,
         // it was most likely caused by an rmdir issued by a user.
         if (!(errno == ENOENT && type == MetaSyncDirType::DentriesHashDir && level == 2))
         {
            LOG(MIRRORING, ERR, "Could not stat dir entry.", candidatePath, sysErr);
            numErrors.increase();
         }

         continue;
      }

      if (!S_ISDIR(statBuf.st_mode))
      {
         LOG(MIRRORING, ERR, "Found a non-dir where only directories are expected.", candidatePath,
               oct(statBuf.st_mode));
         numErrors.increase();
         continue;
      }

      // layout is: (dentries|inodes)/l1/l2/...
      //  -> level 0 correlates with type
      //  -> level 1 is not very interesting, except for reporting
      //  -> level 2 must be synced. if it is a dentry hashdir, its contents must also be crawled.
      if (level == 0)
      {
         crawlDir(candidatePath, type, level + 1);
         continue;
      }

      if (level == 1)
      {
         LOG_DBG(MIRRORING, DEBUG, "Adding hashdir sync candidate.", candidatePath);
         addCandidate(candidatePath, type);

         if (type == MetaSyncDirType::DentriesHashDir)
            crawlDir(candidatePath, type, level + 1);

         continue;
      }

      // so here we read a 2nd level dentry hashdir. crawl that too, add sync candidates for each
      // entry we find - non-directories have already been reported, and the bulk resyncer will
      // take care of the fsids directories.
      numDirsDiscovered.increase();
      LOG_DBG(MIRRORING, DEBUG, "Adding contdir sync candidate.", candidatePath);
      addCandidate(candidatePath, MetaSyncDirType::ContentDir);
   }
}
