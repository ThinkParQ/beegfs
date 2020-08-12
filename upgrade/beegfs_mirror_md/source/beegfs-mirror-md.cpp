#include <common/threading/Atomics.h>

#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/xattr.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/lexical_cast.hpp>

namespace {
struct CloseDirDeleter
{
   void operator()(DIR* dir) const
   {
      if (dir)
         closedir(dir);
   }
};

class FD
{
   public:
      FD() : fd(-1) {}

      explicit FD(int fd) : fd(fd) {}

      FD(const FD&) = delete;
      FD& operator=(const FD&) = delete;

      FD(FD&& other) : fd(-1)
      {
         std::swap(fd, other.fd);
      }

      FD& operator=(FD&& other)
      {
         std::swap(fd, other.fd);
         return *this;
      }

      ~FD()
      {
         if (fd >= 0)
            close(fd);
      }

      int get() const { return fd; }

      int release()
      {
         int result = fd;
         fd = -1;
         return result;
      }

   private:
      int fd;
};

class Directory
{
   public:
      explicit Directory(std::string name) : name(std::move(name)), parent(NULL), subdirsMigrated(0)
      {}

      Directory& addSubdir(std::string name)
      {
         subdirs.emplace_back(new Directory(std::move(name)));
         subdirs.back()->parent = this;
         return *subdirs.back();
      }

      const std::string& getName() const { return name; }
      std::vector<std::unique_ptr<Directory>>& getSubdirs() { return subdirs; }

      Directory* getParent() const { return parent; }

      bool notifySubdirMigrated()
      {
         return subdirsMigrated.increase() + 1 == subdirs.size();
      }

   private:
      std::string name;

      Directory* parent;
      std::vector<std::unique_ptr<Directory>> subdirs;
      Atomic<unsigned> subdirsMigrated;
};

std::ostream& operator<<(std::ostream& os, const Directory& dir)
{
   if (dir.getParent())
      os << *dir.getParent() << '/';

   return os << dir.getName();
}

bool createDirTree(FD fd, Directory& parent, Atomic<uint64_t>& counter, uint64_t maxDepth)
{
   bool allGood = true;

   std::unique_ptr<DIR, CloseDirDeleter> handle(fdopendir(fd.release()));
   if (!handle)
   {
      perror("fdopendir");
      exit(1);
   }

   int dirFD = dirfd(handle.get());

   ssize_t xattrListSize = flistxattr(dirFD, NULL, 0);
   if (xattrListSize > XATTR_LIST_MAX ||
         (xattrListSize < 0 && errno == ERANGE))
   {
      std::cerr << "Cannot migrate " << parent << ": too many xattrs\n";
      allGood = false;
   }
   else if (xattrListSize < 0 && errno != ENOTSUP)
   {
      perror("flistxattr");
      exit(1);
   }

   if (maxDepth == 0)
      return allGood;

   struct dirent* entry;

   rewinddir(handle.get());
   while ((entry = readdir(handle.get())))
   {
      struct stat statData;

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
         continue;

      if (::fstatat(dirFD, entry->d_name, &statData, AT_SYMLINK_NOFOLLOW))
      {
         perror("fstatat");
         exit(1);
      }

      if (!S_ISDIR(statData.st_mode))
      {
         if (statData.st_nlink > 1)
         {
            std::cerr << "Cannot migrate " << parent << '/' << entry->d_name << ": is hardlinked\n";
            allGood = false;
         }
         continue;
      }

      if (maxDepth == 1)
         continue;

      Directory& subdir = parent.addSubdir(entry->d_name);

      FD subdirFD(::openat(dirFD, entry->d_name, O_DIRECTORY | O_RDONLY));
      if (subdirFD.get() < 0)
      {
         perror("openat");
         exit(1);
      }

      counter.increase();
      allGood &= createDirTree(std::move(subdirFD), subdir, counter, maxDepth - 1);
   }

   return allGood;
}

class MigrateWorkQueue
{
   public:
      MigrateWorkQueue(): finished(false) {}

      void addItem(Directory& dir)
      {
         std::lock_guard<std::mutex> _(mtx);

         queue.push_back(&dir);
         cond.notify_one();
      }

      Directory* popItem()
      {
         std::unique_lock<std::mutex> lock(mtx);

         while (!finished && queue.empty())
            cond.wait(lock);

         if (finished)
            return NULL;

         Directory* result = queue.front();
         queue.pop_front();
         return result;
      }

      void finish()
      {
         std::lock_guard<std::mutex> _(mtx);

         finished = true;
         cond.notify_all();
      }

   private:
      std::mutex mtx;
      std::condition_variable cond;

      bool finished;
      std::deque<Directory*> queue;
};

void moveDirectoryContent(int fromFD, int toFD)
{
   std::unique_ptr<DIR, CloseDirDeleter> handle(fdopendir(dup(fromFD)));
   if (!handle)
   {
      perror("dup/fdopendir");
      exit(1);
   }

   struct dirent* entry;

   rewinddir(handle.get());
   while ((entry = readdir(handle.get())))
   {
      struct stat statData;

      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
         continue;

      if (::fstatat(fromFD, entry->d_name, &statData, AT_SYMLINK_NOFOLLOW))
      {
         perror("fstatat");
         exit(1);
      }

      if (renameat(fromFD, entry->d_name, toFD, entry->d_name))
      {
         perror("renameat");
         exit(1);
      }
   }
}

void copyDirAttribs(int dirFD, const std::string& tmpDir)
{
   struct stat statData;

   if (::fstat(dirFD, &statData))
   {
      perror("fstat");
      exit(1);
   }

   if (chown(tmpDir.c_str(), statData.st_uid, statData.st_gid))
   {
      perror("chown");
      exit(1);
   }

   if (chmod(tmpDir.c_str(), statData.st_mode & ~S_IFMT))
   {
      perror("chmod");
      exit(1);
   }

   struct timespec times[2] = {
      statData.st_atim,
      statData.st_mtim,
   };

   if (utimensat(-1, tmpDir.c_str(), times, 0))
   {
      perror("utimensat");
      exit(1);
   }

   char xattrList[XATTR_LIST_MAX];

   ssize_t xattrListSize = flistxattr(dirFD, xattrList, sizeof(xattrList));
   if (xattrListSize < 0)
   {
      perror("flistxattr");
      exit(1);
   }

   const char* xattrName = xattrList;
   while (xattrListSize > 0)
   {
      char xattrValue[XATTR_SIZE_MAX];

      ssize_t xattrSize = fgetxattr(dirFD, xattrName, xattrValue, sizeof(xattrValue));
      if (xattrSize < 0)
      {
         perror("fgetxattr");
         exit(1);
      }

      if (setxattr(tmpDir.c_str(), xattrName, xattrValue, xattrSize, 0) < 0)
      {
         perror("setxattr");
         exit(1);
      }

      xattrListSize -= strlen(xattrName) + 1;
      xattrName += strlen(xattrName) + 1;
   }
}

void migrateDirectory(const std::string& dir, const std::string& baseDir)
{
   FD dirFD(::open(dir.c_str(), O_DIRECTORY | O_RDONLY));
   if (dirFD.get() < 0)
   {
      std::cerr << "Could not open " << dir << ": " << strerror(errno) << '\n';
      exit(1);
   }

   std::string tmpDir = baseDir + "/migrate.XXXXXX";
   if (!mkdtemp(&tmpDir[0]))
   {
      perror("mkdtemp");
      exit(1);
   }

   copyDirAttribs(dirFD.get(), tmpDir);

   FD tmpDirFD(open(tmpDir.c_str(), O_DIRECTORY | O_RDONLY));
   if (tmpDirFD.get() < 0)
   {
      std::cerr << "Could not open " << tmpDir << ": " << strerror(errno) << '\n';
      exit(1);
   }

   moveDirectoryContent(dirFD.get(), tmpDirFD.get());
   dirFD = {};

   if (rmdir(dir.c_str()))
   {
      std::cerr << "Could not replace " << dir << " with " << tmpDir << ": " << strerror(errno)
         << " (1)\n";
      exit(1);
   }

   if (rename(tmpDir.c_str(), dir.c_str()))
   {
      std::cerr << "Could not replace " << dir << " with " << tmpDir << ": " << strerror(errno)
         << " (2)\n";
      exit(1);
   }
}

void enqueueLeafDirs(MigrateWorkQueue& queue, Directory& dir)
{
   if (dir.getSubdirs().empty())
      queue.addItem(dir);
   else
   {
      for (auto it = dir.getSubdirs().begin(); it != dir.getSubdirs().end(); ++it)
         enqueueLeafDirs(queue, **it);
   }
}

std::vector<std::shared_ptr<std::thread>> startMigrateThreads(unsigned count,
   MigrateWorkQueue& queue, const std::string& baseDir, Atomic<unsigned>& running,
   Atomic<uint64_t>& migrated, bool migrateRoot)
{
   struct ops
   {
      static void threadMain(MigrateWorkQueue& queue, const std::string& baseDir,
         Atomic<unsigned>& running, Atomic<uint64_t>& migrated, bool migrateRoot)
      {
         while (true)
         {
            auto* dir = queue.popItem();

            if (!dir)
               break;

            if (!dir->getParent() && !migrateRoot)
            {
               queue.finish();
               continue;
            }

            std::stringstream dirPath;

            dirPath << *dir;
            migrateDirectory(dirPath.str(), baseDir);

            migrated.increase();

            if (!dir->getParent() && migrateRoot)
            {
               queue.finish();
               continue;
            }

            if (dir->getParent()->notifySubdirMigrated())
               queue.addItem(*dir->getParent());
         }

         running.decrease();
      }
   };

   std::vector<std::shared_ptr<std::thread>> result(count);

   running.set(count);

   for (auto it = result.begin(); it != result.end(); ++it)
      it->reset(new std::thread(std::bind(ops::threadMain, std::ref(queue), std::cref(baseDir),
            std::ref(running), std::ref(migrated), migrateRoot)));

   return std::move(result);
}

struct CmdArgs
{
   std::shared_ptr<std::string> baseDir;
   std::shared_ptr<std::string> candidateDir;

   unsigned threads;
   bool recursive;
};

void printUsage(char* argv0, std::ostream& os)
{
   os
      << "Usage: " << basename(argv0) << " <mountpoint> [directory]\n"
      << "\n"
      << "Migrates metadata of files contained in `directory` to the meta-mirror\n"
      << "state of `mountpoint`. If `directory` is omitted, `mountpoint` is used.\n"
      << "\n"
      << "Arguments:\n"
      << "  -h, --help        print this text\n"
      << "  -t, --threads=N   use N threads for metadata migration.\n"
      << "                    (default: 4, recommended: 4 * NUM_META_SERVERS)\n"
      << "  -r, --recursive   migrate the `directory` subtree, not only `directory`\n"
      << "                    itself\n"
      << std::flush;
}

CmdArgs parseArgs(int argc, char* argv[])
{
   static const struct option options[] = {
      { "help",      no_argument,       0, 'h' },
      { "threads",   required_argument, 0, 't' },
      { "recursive", no_argument,       0, 'r' },
      { 0, 0, 0, 0 }
   };

   CmdArgs result;
   int positionalsRead = 0;

   result.threads = 4;
   result.recursive = false;

   while (true)
   {
      int opt = getopt_long(argc, argv, "ht:r", options, NULL);
      if (opt == -1)
         break;

      switch (opt)
      {
         case 'h':
            printUsage(argv[0], std::cout);
            exit(0);
            break;

         case 't':
            if (!boost::conversion::try_lexical_convert(optarg, result.threads))
            {
               printUsage(argv[0], std::cerr);
               exit(1);
            }
            break;

         case 'r':
            result.recursive = true;
            break;

         default:
            printUsage(argv[0], std::cerr);
            exit(0);
            break;
      }
   }

   for (int i = optind ? optind : 1; i < argc; i++)
   {
      positionalsRead++;
      switch (positionalsRead)
      {
         case 1: {
            result.baseDir.reset(new std::string(argv[i]));
            break;
         }

         case 2: {
            result.candidateDir.reset(new std::string(argv[i]));
            break;
         }

         default: {
            printUsage(argv[0], std::cerr);
            exit(1);
         }
      }
   }

   if (!result.baseDir)
   {
      printUsage(argv[0], std::cerr);
      exit(1);
   }

   if (!result.candidateDir)
      result.candidateDir = result.baseDir;

   return result;
}
}

int main(int argc, char* argv[])
{
   CmdArgs args = parseArgs(argc, argv);

   if (geteuid() != 0)
   {
      std::cerr << "This tool requires root access.\n";
      return 1;
   }

   if (args.baseDir->empty() || args.baseDir->at(0) != '/')
   {
      std::cerr << "mountpoint must be absolute\n";
      return 1;
   }

   if (args.candidateDir->empty() || args.candidateDir->at(0) != '/')
   {
      std::cerr << "directory must be absolute\n";
      return 1;
   }

   if (*args.baseDir->rbegin() != '/')
      *args.baseDir += '/';

   if (*args.candidateDir->rbegin() != '/')
      *args.candidateDir += '/';

   if (args.candidateDir->substr(0, args.baseDir->size()) != *args.baseDir)
   {
      std::cerr << "directory must be a subdirectory of mountpoint\n";
      return 1;
   }

   if (access(args.baseDir->c_str(), R_OK))
   {
      std::cerr << "Could not access " << *args.baseDir << ": " << strerror(errno) << "\n";
      return 1;
   }

   if (access(args.candidateDir->c_str(), R_OK))
   {
      std::cerr << "Could not access " << *args.baseDir << ": " << strerror(errno) << "\n";
      return 1;
   }

   const bool migratingFullSystem = *args.candidateDir == *args.baseDir;

   Directory dir(args.candidateDir->substr(0, args.candidateDir->size() - 1));
   Atomic<uint64_t> dirsFound(migratingFullSystem ? 0 : 1);

   {
      Atomic<bool> done(false);
      bool listGood = true;

      FD candidate(open(dir.getName().c_str(), O_DIRECTORY | O_RDONLY));
      if (candidate.get() < 0)
      {
         std::cerr << "Could not open " << dir.getName() << ": " << strerror(errno) << "\n";
         return 1;
      }

      struct ops
      {
         static void threadMain(bool& listGood, int candidate, Directory& dir,
            Atomic<uint64_t>& dirsFound, bool recursive, Atomic<bool>& done)
         {
            listGood = createDirTree(FD(dup(candidate)), dir, dirsFound, recursive ? -1 : 1);
            done = true;
         }
      };

      std::thread tList(std::bind(ops::threadMain, std::ref(listGood), candidate.get(),
            std::ref(dir), std::ref(dirsFound), args.recursive, std::ref(done)));

      do
      {
         std::cout << "Found " << dirsFound.read() << " directories\r" << std::flush;
         usleep(100000);
      } while (!done.read());

      tList.join();
      std::cout << "Found " << dirsFound.read() << " directories\n";

      if (!listGood)
      {
         std::cerr << "Some files cannot be migrated, aborting\n";
         return 1;
      }
   }

   {
      MigrateWorkQueue queue;
      Atomic<unsigned> running;
      Atomic<uint64_t> migrated(0);

      enqueueLeafDirs(queue, dir);
      auto threads = startMigrateThreads(args.threads, queue, *args.baseDir, running,
            migrated, !migratingFullSystem);

      while (running.read() > 0)
      {
         std::cout << "Migrated " << migrated.read() << " of " << dirsFound.read() << '\r'
            << std::flush;
         usleep(100000);
      }

      for (auto it = threads.begin(); it != threads.end(); ++it)
         (*it)->join();

      std::cout << "Migrated " << migrated.read() << " of " << dirsFound.read() << std::endl;
   }
}
