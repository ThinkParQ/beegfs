#include <database/Set.h>
#include <program/Program.h>

#include "FlatTest.h"

class TestSet : public FlatTest {
};

TEST_F(TestSet, open)
{
   // break config
   int cfgFile = ::creat((fileName + ".t").c_str(), 0660);
   ASSERT_GE(cfgFile, 0);
   ASSERT_EQ(::truncate( (this->fileName + ".t").c_str(), 5), 0);
   do {
      try {
         Set<Data> set(this->fileName);
      } catch (const std::runtime_error& e) {
         ASSERT_NE(std::string(e.what()).find("cannot read set"), std::string::npos);
         ASSERT_NE(std::string(e.what()).find("with version 0"), std::string::npos);
         break;
      }

      FAIL();
   } while (false);
   ASSERT_EQ(::write(cfgFile, "v1\n1\n", 5), 5);
   do {
      try {
         Set<Data> set(this->fileName);
      } catch (const std::runtime_error& e) {
         ASSERT_NE(std::string(e.what()).find("bad set description"), std::string::npos);
         break;
      }

      FAIL();
   } while (false);
   ASSERT_EQ(::close(cfgFile), 0);
   ASSERT_EQ(::unlink((this->fileName + ".t").c_str()), 0);

   // reopen with saved config
   {
      Set<Data> set(this->fileName);
   }
   {
      Set<Data> set(this->fileName);
   }

   ASSERT_EQ(::truncate((this->fileName + ".t").c_str(), 5000), 0);
   do {
      try {
         Set<Data> set(this->fileName);
      } catch (const std::runtime_error& e) {
         ASSERT_NE(std::string(e.what()).find("bad set description"), std::string::npos);
         break;
      }

      FAIL();
   } while (false);
}

TEST_F(TestSet, drop)
{
   {
      Set<Data> set(this->fileName);
      set.newFragment();
   }
   {
      Set<Data> set(this->fileName);
      set.drop();
   }

   ASSERT_EQ(::rmdir(this->dirName.c_str()), 0);
}

TEST_F(TestSet, clear)
{
   {
      Set<Data> set(this->fileName);
      set.newFragment();
   }
   {
      Set<Data> set(this->fileName);
      set.clear();
   }

   ASSERT_EQ(::unlink( (this->fileName + ".t").c_str()), 0);
   ASSERT_EQ(::rmdir(this->dirName.c_str()), 0);
}

TEST_F(TestSet, merge)
{
   Data d = {0, {}};

   {
      Set<Data> set1(this->fileName + "1");
      set1.newFragment()->append(d);

      Set<Data> set2(this->fileName + "2");
      set2.newFragment()->append(d);
   }

   {
      Set<Data> set1(this->fileName + "1");
      Set<Data> set2(this->fileName + "2");
      set2.mergeInto(set1);
   }

   {
      Set<Data> set1(this->fileName + "1");
      set1.drop();
   }

   ASSERT_EQ(::unlink((this->fileName + "2.t").c_str()), 0);
   ASSERT_EQ(::rmdir(this->dirName.c_str()), 0);
}

TEST_F(TestSet, dataOps)
{
   static const unsigned FRAG_COUNT = 2 * 3 * 4;

   Set<Data> set(this->fileName);

   ASSERT_FALSE(set.cursor().step());

   {
      for(unsigned i = 0; i < FRAG_COUNT; i++)
      {
         Data d = { i, {} };
         SetFragment<Data>* frag = set.newFragment();

         frag->append(d);
         frag->append(d);

         ASSERT_EQ(set.size(), 2 * (i + 1));
      }
   }

   {
      Set<Data>::Cursor cursor = set.cursor();

      for(unsigned i = 0; i < FRAG_COUNT; i++) {
         ASSERT_TRUE(cursor.step());
         ASSERT_EQ(cursor.get()->id, i);
         ASSERT_TRUE(cursor.step());
         ASSERT_EQ(cursor.get()->id, i);
      }

      ASSERT_FALSE(cursor.step());
   }

   set.makeUnique();
   ASSERT_EQ(set.size(), FRAG_COUNT);

   {
      Set<Data>::Cursor cursor = set.cursor();

      for(unsigned i = 0; i < FRAG_COUNT; i++) {
         ASSERT_TRUE(cursor.step());
         ASSERT_EQ(cursor.get()->id, i);
      }

      ASSERT_FALSE(cursor.step());
   }

   ASSERT_TRUE(set.getByKey(FRAG_COUNT / 2).first);
   ASSERT_EQ(set.getByKey(FRAG_COUNT / 2).second.id, FRAG_COUNT / 2);

   ASSERT_FALSE(set.getByKey(FRAG_COUNT * 2).first);

   struct ops
   {
      static int64_t key(uint64_t key) { return key + 6; }
   };

   ASSERT_TRUE(set.getByKeyProjection(7, ops::key).first);
   ASSERT_EQ(set.getByKeyProjection(7, ops::key).second.id, 1u);
}
