#include "ReadWrite.h"
#include "Config.h"
#include "FaultInject.h"
#include <unistd.h>
#include <cppunit/config/SourcePrefix.h>
#include <sys/mman.h>
#include <sys/stat.h>

static SafeFD openFileAt(const std::string& dir, int flags, FileUnlinker* unlinker = nullptr)
{
   return SafeFD::openAt(dir, "file", flags, unlinker);
}

static void stateFileAt(const std::string& dir)
{
   struct stat s;

   CPPUNIT_ASSERT(::stat( (dir + "/file").c_str(), &s) == 0);
}

void ReadWriteTest::testFileDescription(FileDescription& file, tfd_flags flags)
{
   FileUnlinker unlinkFile1;
   FileUnlinker unlinkFile2;

   bool spanClients = flags & TFD_SPAN;
   bool doUnlink = !(flags & TFD_NO_UNLINK);
   bool doFsync = flags & TFD_SYNC_BY_FSYNC;
   bool noSync = flags & TFD_SYNC_NEVER;

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR,
      doUnlink ? &unlinkFile1 : nullptr);
   file.writeTo(file1);

   if(!noSync)
   {
      if(doFsync)
         CPPUNIT_ASSERT(::fsync(file1.raw() ) == 0);
      else
         file1.close();
   }

   const auto& dir = spanClients ? Config::getDir2() : Config::getDir1();
   auto file2 = openFileAt(dir, O_RDONLY);
   file.verifyFrom(file2);
}

void ReadWriteTest::doSmallFile(tfd_flags flags)
{
   FileDescription file;

   file
      << Data("snafu")
      << Data("\nasdf");

   testFileDescription(file, flags);
}

void ReadWriteTest::doBigFile(tfd_flags flags)
{
   FileDescription file;

   // this size should never be divisible by PAGE_SIZE
   file
      << Data("begin")
      << RandomBlock(1024 * sysconf(_SC_PAGE_SIZE) )
      << Data("end");

   testFileDescription(file, flags);
}

void ReadWriteTest::singleByte()
{
   FileDescription file;

   file << Data("x");

   testFileDescription(file, TFD_DEFAULT);
}

void ReadWriteTest::doSeek(tfd_flags flags)
{
   FileDescription file;

   file
      << Hole(1024 * 1024 * 128)
      << Data("test");

   testFileDescription(file, flags);
}

void ReadWriteTest::seekWriteSyncReread()
{
   FileDescription file;
   FileUnlinker unlink;

   file
      << Hole(1024 * 1024)
      << Data("test");

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);

   file.writeTo(file1);
   CPPUNIT_ASSERT(::fsync(file1.raw() ) == 0);
   CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
   file.verifyFrom(file1);
}

void ReadWriteTest::readIOError()
{
   FileDescription file;
   FileUnlinker unlink;
   char dummy;

   file << RandomBlock(4 * sysconf(_SC_PAGESIZE) );

   {
      auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);
      file.writeTo(file1);
   }

   {
      InjectFault fault("readpage");
      auto file2 = openFileAt(Config::getDir2(), O_RDONLY);
      CPPUNIT_ASSERT(::read(file2.raw(), &dummy, 1) == -1 && errno == EIO);
   }

   {
      auto file2 = openFileAt(Config::getDir2(), O_RDONLY);
      InjectFault fault("readpage");
      CPPUNIT_ASSERT(::read(file2.raw(), &dummy, 1) == -1 && errno == EIO);
   }

   {
      InjectFault fault("readpage");
      auto file2 = openFileAt(Config::getDir1(), O_RDONLY);
      CPPUNIT_ASSERT(::read(file2.raw(), &dummy, 1) == -1 && errno == EIO);
   }

   {
      auto file2 = openFileAt(Config::getDir1(), O_RDONLY);
      InjectFault fault("readpage");
      CPPUNIT_ASSERT(::read(file2.raw(), &dummy, 1) == -1 && errno == EIO);
   }
}

void ReadWriteTest::writeIOError()
{
   FileDescription file;
   file << RandomBlock(4 * sysconf(_SC_PAGESIZE) );

   {
      FileUnlinker unlink;
      auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);
      InjectFault fault("writepage");
      file.writeTo(file1);
      CPPUNIT_ASSERT(file1.close() == -1 && errno == EIO);
   }

   {
      FileUnlinker unlink;
      auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);
      {
         InjectFault fault("writepage");
         file.writeTo(file1);
         CPPUNIT_ASSERT(file1.close() == -1 && errno == EIO);
      }
      auto file2 = openFileAt(Config::getDir1(), O_RDONLY);
      CPPUNIT_ASSERT(::lseek(file2.raw(), 0, SEEK_END) == 0);
   }

   {
      FileUnlinker unlink;
      auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);
      {
         InjectFault fault("writepage");
         file.writeTo(file1);
         CPPUNIT_ASSERT(::fsync(file1.raw() ) == -1 && errno == EIO);
      }
      CPPUNIT_ASSERT(::fsync(file1.raw() ) == 0);
      CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
      file.verifyFrom(file1);
   }
}

void ReadWriteTest::writeSyncWriteMore()
{
   FileDescription fileWrite, fileVerify;
   FileUnlinker unlink;

   fileWrite << Data("a");
   fileVerify << Data("aa");

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_RDWR | O_TRUNC);
   fileWrite.writeTo(file1);
   CPPUNIT_ASSERT(::fsync(file1.raw() ) == 0);
   fileWrite.writeTo(file1);

   CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
   fileVerify.verifyFrom(file1);
}

void ReadWriteTest::concurrentPageModify()
{
   FileUnlinker unlink;

   {
      FileDescription base;
      base << Data("AB");

      auto fileB = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);
      base.writeTo(fileB);
   }

   {
      char c1 = 'T';

      auto file1 = openFileAt(Config::getDir1(), O_RDWR);
      auto file2 = openFileAt(Config::getDir2(), O_RDWR);

      CPPUNIT_ASSERT(::lseek(file2.raw(), 1, SEEK_SET) == 1);
      CPPUNIT_ASSERT(::write(file1.raw(), &c1, 1) == 1);
      CPPUNIT_ASSERT(::write(file2.raw(), &c1, 1) == 1);
   }

   {
      FileDescription expect;
      expect << Data("TT");

      auto file1 = openFileAt(Config::getDir1(), O_RDONLY);
      expect.verifyFrom(file1);
   }
}

void ReadWriteTest::concurrentAppend()
{
   FileUnlinker unlink;

   {
      auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_APPEND | O_RDWR, &unlink);
      auto file2 = openFileAt(Config::getDir2(), O_APPEND | O_RDWR);

      Data("a").writeTo(file1);
      Data("b").writeTo(file2);
   }

   char buf[2];
   auto fileV = openFileAt(Config::getDir1(), O_RDONLY);

   CPPUNIT_ASSERT(::read(fileV.raw(), buf, 2) == 2);
   CPPUNIT_ASSERT( (buf[0] == 'a' && buf[1] == 'b') || (buf[0] == 'b' || buf[1] == 'a') );
}

void ReadWriteTest::readWriteSpanDirect()
{
   FileUnlinker unlink;

   FileDescription part1;
   part1 << Data('1', 512);

   FileDescription part2;
   part2 << Data('2', 512);

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR | O_DIRECT, &unlink);
   auto file2 = openFileAt(Config::getDir2(), O_RDWR | O_DIRECT);

   part1.writeTo(file1);
   CPPUNIT_ASSERT(::lseek(file2.raw(), 512, SEEK_SET) == 512);
   part2.writeTo(file2);
   CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
   CPPUNIT_ASSERT(::lseek(file2.raw(), 0, SEEK_SET) == 0);

   FileDescription expect;
   expect << Data('1', 512) << Data('2', 512);

   expect.verifyFrom(file1);
   expect.verifyFrom(file2);
}

void ReadWriteTest::manySmallWrites()
{
   FileUnlinker unlink;

   FileDescription part;
   part << Data('A', 1);

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);

   for(int i = 0; i < 256; i++)
      part.writeTo(file1);

   CPPUNIT_ASSERT(::lseek(file1.raw(), 1, SEEK_CUR) == 257);

   for(int i = 0; i < 256; i++)
      part.writeTo(file1);

   FileDescription expect;
   expect << Data('A', 256) << Data(0, 1) << Data('A', 256);

   CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
   expect.verifyFrom(file1);
}

void ReadWriteTest::largeSparseDirect()
{
   FileUnlinker unlink;

   FileDescription file;
   file
      << RandomBlock(sysconf(_SC_PAGESIZE) )
      << Hole(1024 * sysconf(_SC_PAGESIZE) )
      << RandomBlock(sysconf(_SC_PAGESIZE) );

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR | O_DIRECT, &unlink);

   file.writeTo(file1);
   CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
   file.verifyFrom(file1);
}

void ReadWriteTest::mapModifyMap()
{
   FileUnlinker unlink;

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);
   CPPUNIT_ASSERT(::ftruncate(file1.raw(), sysconf(_SC_PAGESIZE) ) == 0);

   auto mapAddr = (char*) ::mmap(nullptr, sysconf(_SC_PAGESIZE), PROT_READ | PROT_WRITE,
      MAP_SHARED, file1.raw(), 0);
   CPPUNIT_ASSERT(mapAddr != MAP_FAILED);

   mapAddr[23] = 42;

   auto file2 = openFileAt(Config::getDir1(), O_RDWR);
   auto mapAddr2 = (char*) ::mmap(nullptr, sysconf(_SC_PAGESIZE), PROT_READ | PROT_WRITE,
      MAP_SHARED, file2.raw(), 0);
   CPPUNIT_ASSERT(mapAddr2 != MAP_FAILED);
   CPPUNIT_ASSERT(mapAddr2[23] == 42);

   CPPUNIT_ASSERT(::munmap(mapAddr2, sysconf(_SC_PAGESIZE) ) == 0);
   CPPUNIT_ASSERT(::munmap(mapAddr, sysconf(_SC_PAGESIZE) ) == 0);
}

void ReadWriteTest::truncate()
{
   FileUnlinker unlink;

   FileDescription data, truncated;

   data << Data("data");
   truncated << Data({'d', 'a', 't', 0});

   auto file = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);

   data.writeTo(file);

   CPPUNIT_ASSERT(::ftruncate(file.raw(), 3) == 0);
   CPPUNIT_ASSERT(::ftruncate(file.raw(), 4) == 0);

   CPPUNIT_ASSERT(::lseek(file.raw(), 0, SEEK_SET) == 0);
   truncated.verifyFrom(file);
}

void ReadWriteTest::appendTruncateAppend()
{
   FileUnlinker unlink;

   FileDescription data;
   data << Data("check");

   auto file = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR | O_APPEND, &unlink);
   data.writeTo(file);
   CPPUNIT_ASSERT(::ftruncate(file.raw(), 0) == 0);
   data.writeTo(file);

   CPPUNIT_ASSERT(::lseek(file.raw(), 0, SEEK_SET) == 0);
   data.verifyFrom(file);
}

void ReadWriteTest::appendWriteAppend()
{
   FileUnlinker unlink;

   FileDescription append, write;
   append << Data("A");
   write << Data("W");

   {
      auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR | O_APPEND, &unlink);
      auto file2 = openFileAt(Config::getDir1(), O_RDWR);

      append.writeTo(file1);
      CPPUNIT_ASSERT(::lseek(file2.raw(), 1, SEEK_SET) == 1);
      write.writeTo(file2);
      append.writeTo(file1);
   }

   FileDescription expect;
   expect << Data("AWA");

   auto file = openFileAt(Config::getDir1(), O_RDWR);
   expect.verifyFrom(file);
}

void ReadWriteTest::noIsizeDecrease()
{
   FileUnlinker unlink;

   FileDescription write, read;
   write << Data("test");
   read << Data("testtesttest");

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);

   write.writeTo(file1);
   CPPUNIT_ASSERT(::fsync(file1.raw() ) == 0);

   write.writeTo(file1);

   stateFileAt(Config::getDir1() );

   write.writeTo(file1);

   CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
   read.verifyFrom(file1);
}

void ReadWriteTest::writeCacheBypass()
{
   FileUnlinker unlink;
   FileDescription random, zero;

   zero << Data(0, 4 * sysconf(_SC_PAGESIZE) );
   random << RandomBlock(4 * sysconf(_SC_PAGESIZE) );

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);

   zero.writeTo(file1);

   {
      InjectFault fault("write_force_cache_bypass", true);
      CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
      random.writeTo(file1);
   }

   CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
   random.verifyFrom(file1);
}

void ReadWriteTest::readCacheBypass()
{
   FileUnlinker unlink;
   FileDescription initial, final;

   initial << RandomBlock(1 * sysconf(_SC_PAGESIZE) );
   final << RandomBlock(1 * sysconf(_SC_PAGESIZE) );

   auto file1 = openFileAt(Config::getDir1(), O_CREAT | O_TRUNC | O_RDWR, &unlink);

   initial.writeTo(file1);
   CPPUNIT_ASSERT(::fsync(file1.raw() ) == 0);

   CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
   final.writeTo(file1);

   {
      InjectFault fault("read_force_cache_bypass", true);
      CPPUNIT_ASSERT(::lseek(file1.raw(), 0, SEEK_SET) == 0);
      final.verifyFrom(file1);
   }
}
