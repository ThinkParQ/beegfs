#include "TestSetFragment.h"
#include <tests/TestRunnerBase.h>

#include <database/SetFragment.h>
#include <program/Program.h>

REGISTER_TEST_SUITE(TestSetFragment);

void TestSetFragment::testOpen()
{
   if(::getuid() == 0)
   {
      std::cerr << "Test run as root, skipping DAC tests\n";
   }
   else
   {
      // bad directory permissions (can't open or create)
      CPPUNIT_ASSERT(::chmod(this->dirName.c_str(), 0) == 0);
      do {
         try {
            SetFragment<Data> frag(this->fileName);
         } catch (const std::runtime_error& e) {
            CPPUNIT_ASSERT(std::string(e.what() ) == "could not open fragment file");
            break;
         }

         CPPUNIT_FAIL("");
      } while (0);
      CPPUNIT_ASSERT(::chmod(this->dirName.c_str(), 0770) == 0);
   }

   // not a regular file
   CPPUNIT_ASSERT(::mkfifo(this->fileName.c_str(), 0660) == 0);
   do {
      try {
         SetFragment<Data> frag(this->fileName);
      } catch (const std::runtime_error& e) {
         CPPUNIT_ASSERT(std::string(e.what() ) == "error while opening fragment file");
         break;
      }

      CPPUNIT_FAIL("");
   } while (0);
   CPPUNIT_ASSERT(::unlink(this->fileName.c_str() ) == 0);

   // corrupted config area
   CPPUNIT_ASSERT(::close(::creat(this->fileName.c_str(), 0660) ) == 0);
   CPPUNIT_ASSERT(::truncate(this->fileName.c_str(), SetFragment<Data>::CONFIG_AREA_SIZE - 1) == 0);
   do {
      try {
         SetFragment<Data> frag(this->fileName);
      } catch (const std::runtime_error& e) {
         CPPUNIT_ASSERT(std::string(e.what() ) == "error while opening fragment file");
         break;
      }

      CPPUNIT_FAIL("");
   } while (0);
   CPPUNIT_ASSERT(::unlink(this->fileName.c_str() ) == 0);

   // corrupted data
   CPPUNIT_ASSERT(::close(::creat(this->fileName.c_str(), 0660) ) == 0);
   CPPUNIT_ASSERT(::truncate(this->fileName.c_str(),
      SetFragment<Data>::CONFIG_AREA_SIZE + sizeof(Data) - 1) == 0);
   do {
      try {
         SetFragment<Data> frag(this->fileName);
      } catch (const std::runtime_error& e) {
         CPPUNIT_ASSERT(std::string(e.what() ) == "error while opening fragment file");
         break;
      }

      CPPUNIT_FAIL("");
   } while (0);
   CPPUNIT_ASSERT(::unlink(this->fileName.c_str() ) == 0);

   // should work
   SetFragment<Data> frag(this->fileName);
}

void TestSetFragment::testDrop()
{
   struct stat stat;

   {
      SetFragment<Data> frag(this->fileName);
   }

   CPPUNIT_ASSERT(::stat(this->fileName.c_str(), &stat) == 0);

   {
      SetFragment<Data> frag(this->fileName);
      frag.drop();
   }

   CPPUNIT_ASSERT(::stat(this->fileName.c_str(), &stat) == -1 && errno == ENOENT);
}

void TestSetFragment::testFlush()
{
   SetFragment<Data> frag(this->fileName);
   Data d = {0, {}};
   struct stat stat;

   frag.append(d);

   CPPUNIT_ASSERT(::stat(this->fileName.c_str(), &stat) == 0);
   CPPUNIT_ASSERT(stat.st_size == 0);

   frag.flush();

   CPPUNIT_ASSERT(::stat(this->fileName.c_str(), &stat) == 0);
   CPPUNIT_ASSERT(stat.st_size == frag.CONFIG_AREA_SIZE + sizeof(d));
}

void TestSetFragment::testAppendAndAccess()
{
   struct ops
   {
      static void fill(SetFragment<Data>& frag, unsigned limit)
      {
         Data d = {0, {}};

         for (unsigned i = 0; i < limit; i++) {
            d.id = i;
            frag.append(d);
         }
      }

      static void check(SetFragment<Data>& frag, unsigned limit)
      {
         CPPUNIT_ASSERT(frag.size() == limit);

         for (unsigned i = 0; i < limit; i++) {
            CPPUNIT_ASSERT(frag[i].id == i);
            CPPUNIT_ASSERT(
               std::count(
                  frag[i].dummy,
                  frag[i].dummy + sizeof(frag[i].dummy),
                  0)
               == sizeof(frag[i].dummy) );
         }
      }
   };

   static const unsigned LIMIT = 2 * SetFragment<Data>::BUFFER_SIZE / sizeof(Data);

   {
      SetFragment<Data> frag(this->fileName);

      ops::fill(frag, LIMIT);
      ops::check(frag, LIMIT);
   }

   // read everything again, with a new instance
   {
      SetFragment<Data> frag(this->fileName);

      ops::check(frag, LIMIT);
   }
}

void TestSetFragment::testSortAndGet()
{
   SetFragment<Data> frag(this->fileName);

   // generate a pseudo-random sequence as {0, 1, ...} * 4001 mod 4003 (both prime)
   for (unsigned i = 0; i < 4003; i++)
   {
      Data d = { i * 4001 % 4003, {} };
      frag.append(d);
   }

   frag.sort();

   for (unsigned i = 0; i < 4003; i++)
      CPPUNIT_ASSERT(frag[i].id == i);

   CPPUNIT_ASSERT(frag.getByKey(17).first);
   CPPUNIT_ASSERT(frag.getByKey(17).second.id == 17);

   CPPUNIT_ASSERT(!frag.getByKey(170000).first);

   struct ops
   {
      static int64_t key(uint64_t key) { return key + 6; }
   };

   CPPUNIT_ASSERT(frag.getByKeyProjection(23, ops::key).first);
   CPPUNIT_ASSERT(frag.getByKeyProjection(23, ops::key).second.id == 17);
}

void TestSetFragment::testRename()
{
   SetFragment<Data> frag(this->fileName);

   frag.rename(this->fileName + "1");

   CPPUNIT_ASSERT(::close(::open( (this->fileName + "1").c_str(), O_RDWR) ) == 0);

   frag.rename(this->fileName);
}
