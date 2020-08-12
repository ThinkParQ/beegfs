#include <database/SetFragment.h>
#include <program/Program.h>

#include "FlatTest.h"

class TestSetFragment : public FlatTest {
};

TEST_F(TestSetFragment, open)
{
   if(::getuid() == 0)
   {
      std::cerr << "Test run as root, skipping DAC tests\n";
   }
   else
   {
      // bad directory permissions (can't open or create)
      ASSERT_EQ(::chmod(this->dirName.c_str(), 0), 0);
      do {
         try {
            SetFragment<Data> frag(this->fileName);
         } catch (const std::runtime_error& e) {
            ASSERT_NE(std::string(e.what()).find("could not open fragment file"), std::string::npos);
            break;
         }

         FAIL();
      } while (false);
      ASSERT_EQ(::chmod(this->dirName.c_str(), 0770), 0);
   }

   // not a regular file
   ASSERT_EQ(::mkfifo(this->fileName.c_str(), 0660), 0);
   do {
      try {
         SetFragment<Data> frag(this->fileName);
      } catch (const std::runtime_error& e) {
         ASSERT_NE(std::string(e.what()).find("error while opening fragment file"), std::string::npos);
         break;
      }

      FAIL();
   } while (false);
   ASSERT_EQ(::unlink(this->fileName.c_str()), 0);

   // corrupted config area
   ASSERT_EQ(::close(::creat(this->fileName.c_str(), 0660)), 0);
   ASSERT_EQ(::truncate(this->fileName.c_str(), SetFragment<Data>::CONFIG_AREA_SIZE - 1), 0);
   do {
      try {
         SetFragment<Data> frag(this->fileName);
      } catch (const std::runtime_error& e) {
         ASSERT_NE(std::string(e.what()).find("error while opening fragment file"), std::string::npos);
         break;
      }

      FAIL();
   } while (false);
   ASSERT_EQ(::unlink(this->fileName.c_str()), 0);

   // corrupted data
   ASSERT_EQ(::close(::creat(this->fileName.c_str(), 0660)), 0);
   ASSERT_EQ(::truncate(this->fileName.c_str(),
      SetFragment<Data>::CONFIG_AREA_SIZE + sizeof(Data) - 1), 0);
   do {
      try {
         SetFragment<Data> frag(this->fileName);
      } catch (const std::runtime_error& e) {
         ASSERT_NE(std::string(e.what()).find("error while opening fragment file"), std::string::npos);
         break;
      }

      FAIL();
   } while (false);
   ASSERT_EQ(::unlink(this->fileName.c_str()), 0);

   // should work
   SetFragment<Data> frag(this->fileName);
}

TEST_F(TestSetFragment, drop)
{
   struct stat stat;

   {
      SetFragment<Data> frag(this->fileName);
   }

   ASSERT_EQ(::stat(this->fileName.c_str(), &stat), 0);

   {
      SetFragment<Data> frag(this->fileName);
      frag.drop();
   }

   ASSERT_EQ(::stat(this->fileName.c_str(), &stat), -1);
   ASSERT_EQ(errno, ENOENT);
}

TEST_F(TestSetFragment, flush)
{
   SetFragment<Data> frag(this->fileName);
   Data d = {0, {}};
   struct stat stat;

   frag.append(d);

   ASSERT_EQ(::stat(this->fileName.c_str(), &stat), 0);
   ASSERT_EQ(stat.st_size, 0);

   frag.flush();

   ASSERT_EQ(::stat(this->fileName.c_str(), &stat), 0);
   ASSERT_EQ(size_t(stat.st_size), frag.CONFIG_AREA_SIZE + sizeof(d));
}

TEST_F(TestSetFragment, appendAndAccess)
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
         ASSERT_EQ(frag.size(), limit);

         for (unsigned i = 0; i < limit; i++) {
            ASSERT_EQ(frag[i].id, i);
            ASSERT_EQ(
               size_t(std::count(
                  frag[i].dummy,
                  frag[i].dummy + sizeof(frag[i].dummy),
                  0)),
               sizeof(frag[i].dummy));
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

TEST_F(TestSetFragment, sortAndGet)
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
      ASSERT_EQ(frag[i].id, i);

   ASSERT_TRUE(frag.getByKey(17).first);
   ASSERT_EQ(frag.getByKey(17).second.id, 17u);

   ASSERT_TRUE(!frag.getByKey(170000).first);

   struct ops
   {
      static int64_t key(uint64_t key) { return key + 6; }
   };

   ASSERT_TRUE(frag.getByKeyProjection(23, ops::key).first);
   ASSERT_EQ(frag.getByKeyProjection(23, ops::key).second.id, 17u);
}

TEST_F(TestSetFragment, rename)
{
   SetFragment<Data> frag(this->fileName);

   frag.rename(this->fileName + "1");

   ASSERT_EQ(::close(::open( (this->fileName + "1").c_str(), O_RDWR)), 0);

   frag.rename(this->fileName);
}
