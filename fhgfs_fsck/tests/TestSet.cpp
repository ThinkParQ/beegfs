#include "TestSet.h"
#include <tests/TestRunnerBase.h>

#include <database/Set.h>
#include <program/Program.h>

REGISTER_TEST_SUITE(TestSet);

void TestSet::testOpen()
{
   // break config
   int cfgFile = ::creat((fileName + ".t").c_str(), 0660);
   CPPUNIT_ASSERT(cfgFile >= 0);
   CPPUNIT_ASSERT(::truncate( (this->fileName + ".t").c_str(), 5) == 0);
   do {
      try {
         Set<Data> set(this->fileName);
      } catch (const std::runtime_error& e) {
         CPPUNIT_ASSERT(std::string(e.what() ) == "cannot read this database version");
         break;
      }

      CPPUNIT_FAIL("");
   } while (0);
   CPPUNIT_ASSERT(::write(cfgFile, "v1\n1\n", 5) == 5);
   do {
      try {
         Set<Data> set(this->fileName);
      } catch (const std::runtime_error& e) {
         CPPUNIT_ASSERT(std::string(e.what() ) == "bad set description");
         break;
      }

      CPPUNIT_FAIL("");
   } while (0);
   CPPUNIT_ASSERT(::close(cfgFile) == 0);
   CPPUNIT_ASSERT(::unlink( (this->fileName + ".t").c_str() ) == 0);

   // reopen with saved config
   {
      Set<Data> set(this->fileName);
   }
   {
      Set<Data> set(this->fileName);
   }

   CPPUNIT_ASSERT(::truncate( (this->fileName + ".t").c_str(), 5000) == 0);
   do {
      try {
         Set<Data> set(this->fileName);
      } catch (const std::runtime_error& e) {
         CPPUNIT_ASSERT(std::string(e.what() ) == "bad set description");
         break;
      }

      CPPUNIT_FAIL("");
   } while (0);
}

void TestSet::testDrop()
{
   {
      Set<Data> set(this->fileName);
      set.newFragment();
   }
   {
      Set<Data> set(this->fileName);
      set.drop();
   }

   CPPUNIT_ASSERT(::rmdir(this->dirName.c_str() ) == 0);
}

void TestSet::testClear()
{
   {
      Set<Data> set(this->fileName);
      set.newFragment();
   }
   {
      Set<Data> set(this->fileName);
      set.clear();
   }

   CPPUNIT_ASSERT(::unlink( (this->fileName + ".t").c_str() ) == 0);
   CPPUNIT_ASSERT(::rmdir(this->dirName.c_str() ) == 0);
}

void TestSet::testMerge()
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

   CPPUNIT_ASSERT(::unlink( (this->fileName + "2.t").c_str() ) == 0);
   CPPUNIT_ASSERT(::rmdir(this->dirName.c_str() ) == 0);
}

void TestSet::testDataOps()
{
   static const unsigned FRAG_COUNT = 2 * 3 * 4;

   Set<Data> set(this->fileName);

   CPPUNIT_ASSERT(!set.cursor().step() );

   {
      for(unsigned i = 0; i < FRAG_COUNT; i++)
      {
         Data d = { i, {} };
         SetFragment<Data>* frag = set.newFragment();

         frag->append(d);
         frag->append(d);

         CPPUNIT_ASSERT(set.size() == 2 * (i + 1) );
      }
   }

   {
      Set<Data>::Cursor cursor = set.cursor();

      for(unsigned i = 0; i < FRAG_COUNT; i++) {
         CPPUNIT_ASSERT(cursor.step() );
         CPPUNIT_ASSERT(cursor.get()->id == i);
         CPPUNIT_ASSERT(cursor.step() );
         CPPUNIT_ASSERT(cursor.get()->id == i);
      }

      CPPUNIT_ASSERT(!cursor.step() );
   }

   set.makeUnique();
   CPPUNIT_ASSERT(set.size() == FRAG_COUNT);

   {
      Set<Data>::Cursor cursor = set.cursor();

      for(unsigned i = 0; i < FRAG_COUNT; i++) {
         CPPUNIT_ASSERT(cursor.step() );
         CPPUNIT_ASSERT(cursor.get()->id == i);
      }

      CPPUNIT_ASSERT(!cursor.step() );
   }

   CPPUNIT_ASSERT(set.getByKey(FRAG_COUNT / 2).first);
   CPPUNIT_ASSERT(set.getByKey(FRAG_COUNT / 2).second.id == FRAG_COUNT / 2);

   CPPUNIT_ASSERT(!set.getByKey(FRAG_COUNT * 2).first);

   struct ops
   {
      static int64_t key(uint64_t key) { return key + 6; }
   };

   CPPUNIT_ASSERT(set.getByKeyProjection(7, ops::key).first);
   CPPUNIT_ASSERT(set.getByKeyProjection(7, ops::key).second.id == 1);
}
