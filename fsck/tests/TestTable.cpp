#include "TestTable.h"

void TestTable::SetUp()
{
   FlatTest::SetUp();
   this->table.reset(new Table<Data>(this->fileName, 4096) );
}

void TestTable::TearDown()
{
   if(this->table)
      this->table->drop();

   FlatTest::TearDown();
}

TEST_F(TestTable, dataHandling)
{
   Data d = {0, {}};

   // first, only ordered inserts
   d.id = 0;
   this->table->insert(d);
   d.id = 1;
   this->table->insert(d);

   // complete the base set
   this->table->commitChanges();

   // insert "in order"
   d.id = 2;
   this->table->insert(d);

   // unordered delete should fail
   ASSERT_THROW(this->table->remove(1), std::runtime_error);

   // ordered delete should work
   this->table->remove(3);

   // reinsert of same id should work
   d.id = 3;
   this->table->insert(d);

   // unordered insert should now fail
   ASSERT_THROW(this->table->insert(d), std::runtime_error);

   // reset changes
   this->table->commitChanges();

   // unordered insert should work
   d.id = 0;
   this->table->insert(d);
   d.id = 4;
   this->table->insert(d);
   d.id = 1;
   this->table->insert(d);

   // delete should fail
   ASSERT_THROW(this->table->remove(1), std::runtime_error);

   // reset changes
   this->table->commitChanges();

   // ordered delete should work
   this->table->remove(1);
   this->table->remove(2);
   this->table->remove(3);

   // unordered insert should fail
   d.id = 2;
   ASSERT_THROW(this->table->insert(d), std::runtime_error);

   // unordered delete should work
   this->table->remove(3);

   // insert should fail after unordered delete
   d.id = 10;
   ASSERT_THROW(this->table->insert(d), std::runtime_error);

   // uncomitted changes should yield the sequence [0, 0, 1, 1, 2, 3, 4]
   {
      Table<Data>::QueryType cursor = this->table->cursor();

      unsigned expect[7] = { 0, 0, 1, 1, 2, 3, 4 };
      for(unsigned i = 0; i < 7; i++)
      {
         ASSERT_TRUE(cursor.step());
         ASSERT_EQ(cursor.get()->id, expect[i]);
      }

      ASSERT_FALSE(cursor.step());
   }

   // commited should contain the sequence [0, 0, 4]
   this->table->commitChanges();
   {
      Table<Data>::QueryType cursor = this->table->cursor();

      ASSERT_TRUE(cursor.step());
      ASSERT_EQ(cursor.get()->id, 0u);
      ASSERT_TRUE(cursor.step());
      ASSERT_EQ(cursor.get()->id, 0u);
      ASSERT_TRUE(cursor.step());
      ASSERT_EQ(cursor.get()->id, 4u);
      ASSERT_FALSE(cursor.step());
   }

   ASSERT_TRUE(this->table->getByKey(4).first);
   ASSERT_EQ(this->table->getByKey(4).second.id, 4u);

   ASSERT_FALSE(this->table->getByKey(3).first);

   struct ops
   {
      static int64_t key(uint64_t key) { return key + 6; }
   };

   ASSERT_TRUE(this->table->getByKeyProjection(10, ops::key).first);
   ASSERT_EQ(this->table->getByKeyProjection(10, ops::key).second.id, 4u);
}

TEST_F(TestTable, bulkInsert)
{
   {
      boost::shared_ptr<Buffer<Data> > buf1, buf2;

      buf1 = this->table->bulkInsert();
      buf2 = this->table->bulkInsert();

      Data d = { 0, {} };

      d.id = 1;
      buf1->append(d);

      d.id = 2;
      buf2->append(d);
   }

   // all insert must have gone to base, queryable immediatly
   {
      Table<Data>::QueryType cursor = this->table->cursor();

      ASSERT_TRUE(cursor.step());
      ASSERT_EQ(cursor.get()->id, 1u);
      ASSERT_TRUE(cursor.step());
      ASSERT_EQ(cursor.get()->id, 2u);
      ASSERT_FALSE(cursor.step());
   }

   // another bulk insert is fine, no modifications happened yet
   {
      boost::shared_ptr<Buffer<Data> > buf;

      buf = this->table->bulkInsert();

      Data d = { 3, {} };
      buf->append(d);

      // can't do normal insert or remove during bulk insert
      ASSERT_THROW(this->table->insert(d), std::runtime_error);
      ASSERT_THROW(this->table->remove(4), std::runtime_error);
   }

   // all insert must have gone to base, queryable immediatly
   {
      Table<Data>::QueryType cursor = this->table->cursor();

      ASSERT_TRUE(cursor.step());
      ASSERT_EQ(cursor.get()->id, 1u);
      ASSERT_TRUE(cursor.step());
      ASSERT_EQ(cursor.get()->id, 2u);
      ASSERT_TRUE(cursor.step() );
      ASSERT_EQ(cursor.get()->id, 3u);
      ASSERT_FALSE(cursor.step());
   }

   this->table->remove(1);
   ASSERT_THROW(this->table->bulkInsert(), std::runtime_error);

   this->table->commitChanges();
   ASSERT_THROW(this->table->bulkInsert(), std::runtime_error);
}
