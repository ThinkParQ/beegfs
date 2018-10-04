#include "MemoryOrdering.h"
#include "Config.h"
#include "SafeFD.h"
#include "FileDescription.h"
#include "FileUnlinker.h"
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>

static int fcntl_lock_file(const SafeFD& file, int lockType, unsigned len)
{
   struct flock lock;

   lock.l_type = lockType;
   lock.l_whence = SEEK_SET;
   lock.l_start = 0;
   lock.l_len = len;

   return ::fcntl(file.raw(), F_SETLKW, &lock);
}

void MemoryOrdering::consistency()
{
   enum rel_op_t
   { REL_CLOSE, REL_FSYNC, REL_SFR, REL_FUNLOCK, REL_FCNTL_UNLOCK, REL_MSYNC, REL_MAX, };
   static const char *relNames[] = {
      "close",
      "fsync",
      "sync_file_range",
      "flock(LOCK_UN)",
      "fcntl(F_UNLCK)",
      "msync",
   };

   enum acq_op_t
   { ACQ_OPEN, ACQ_FLOCK_SH, ACQ_FCNTL_LOCK_RD, ACQ_MMAP, ACQ_MAX, };
   static const char *acqNames[] = {
      "open",
      "flock(LOCK_SH)",
      "fcntl(F_RDLCK)",
      "mmap",
   };

   FileDescription file;

   file
      << Data("begin")
      << RandomBlock(sysconf(_SC_PAGESIZE) )
      << Data("end");

   std::cout << "(\n";
   for(int relOp = REL_CLOSE; relOp < REL_MAX; relOp++)
   {
      for(int acqOp = ACQ_OPEN; acqOp < ACQ_MAX; acqOp++)
      {
         std::cout << "\t * " << relNames[relOp] << " > " << acqNames[acqOp];

         FileUnlinker unlink;

         auto file1 = SafeFD::openAt(Config::getDir1(), "file", O_CREAT | O_TRUNC | O_RDWR,
            &unlink);
         auto file2 = SafeFD::openAt(Config::getDir2(), "file", O_RDONLY);

         switch(relOp)
         {
            case REL_CLOSE:
               file.writeTo(file1);
               file1.close();
               break;

            case REL_FSYNC:
               file.writeTo(file1);
               CPPUNIT_ASSERT(::fsync(file1.raw()) == 0);
               break;

            case REL_SFR:
            {
               file.writeTo(file1);
               int flags = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE
                  | SYNC_FILE_RANGE_WAIT_AFTER;
               CPPUNIT_ASSERT(::sync_file_range(file1.raw(), 0, 0, flags) == 0);
               break;
            }

            case REL_FUNLOCK:
               CPPUNIT_ASSERT(::flock(file1.raw(), LOCK_EX) == 0);
               file.writeTo(file1);
               CPPUNIT_ASSERT(::flock(file1.raw(), LOCK_UN) == 0);
               break;

            case REL_FCNTL_UNLOCK:
               CPPUNIT_ASSERT(fcntl_lock_file(file1, F_RDLCK, file.size() ) == 0);
               file.writeTo(file1);
               CPPUNIT_ASSERT(fcntl_lock_file(file1, F_UNLCK, file.size() ) == 0);
               break;

            case REL_MSYNC:
            {
               CPPUNIT_ASSERT(::ftruncate(file1.raw(), file.size() ) == 0);
               void* addr = ::mmap(nullptr, file.size(), PROT_READ | PROT_WRITE, MAP_SHARED,
                  file1.raw(), 0);
               CPPUNIT_ASSERT(addr != MAP_FAILED);
               file.writeTo(addr);
               CPPUNIT_ASSERT(::msync(addr, file.size(), MS_SYNC | MS_INVALIDATE) == 0);
               munmap(addr, file.size() );
               break;
            }
         }

         switch(acqOp)
         {
            case ACQ_OPEN:
               file2 = SafeFD::openAt(Config::getDir2(), "file", O_RDONLY);
               file.verifyFrom(file2);
               break;

            case ACQ_FLOCK_SH:
               CPPUNIT_ASSERT(::flock(file2.raw(), LOCK_SH) == 0);
               file.verifyFrom(file2);
               CPPUNIT_ASSERT(::flock(file2.raw(), LOCK_UN) == 0);
               break;

            case ACQ_FCNTL_LOCK_RD:
               CPPUNIT_ASSERT(fcntl_lock_file(file2, F_RDLCK, file.size() ) == 0);
               file.verifyFrom(file2);
               CPPUNIT_ASSERT(fcntl_lock_file(file2, F_UNLCK, file.size() ) == 0);
               break;

            case ACQ_MMAP:
            {
               void* addr = ::mmap(nullptr, file.size(), PROT_READ, MAP_SHARED, file2.raw(), 0);
               CPPUNIT_ASSERT(addr != MAP_FAILED);
               file.verifyFrom(addr);
               munmap(addr, file.size() );
               break;
            }
         }

         std::cout << ": OK" << std::endl;
      }
   }
   std::cout << ")";
}
