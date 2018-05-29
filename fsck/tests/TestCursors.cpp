#include <database/Distinct.h>
#include <database/Filter.h>
#include <database/Group.h>
#include <database/LeftJoinEq.h>
#include <database/Select.h>
#include <database/Union.h>
#include <database/VectorSource.h>

#include <boost/static_assert.hpp>

#include <gtest/gtest.h>

template<typename Obj, size_t Size>
static VectorSource<Obj> vectorSource(const Obj (&data)[Size])
{
   std::vector<Obj> vector(data, data + Size);

   return VectorSource<Obj>(vector);
}

template<typename Source, typename Data, size_t Size, typename EqFn>
static void expect(const Source& source, const Data (&data)[Size], EqFn eqFn)
{
   BOOST_STATIC_ASSERT(Size > 2);

   for(size_t markPos = 0; markPos < Size; markPos++)
   {
      boost::shared_ptr<typename Source::MarkerType> mark;
      Source current = source;

      for(size_t i = 0; i < Size; i++)
      {
         ASSERT_TRUE(current.step());

         if(i == markPos)
            mark = boost::make_shared<typename Source::MarkerType>(current.mark() );

         ASSERT_TRUE(eqFn(*current.get(), data[i]));
      }

      ASSERT_FALSE(current.step());

      current.restore(*mark);

      for(size_t i = markPos; i < Size; i++)
      {
         ASSERT_TRUE(eqFn(*current.get(), data[i]));

         if(i < Size - 1) {
            ASSERT_TRUE(current.step());
         }
      }

      ASSERT_FALSE(current.step());
   }
}

template<typename Source, typename Data, size_t Size>
static void expect(Source source, const Data (&data)[Size])
{
   expect(source, data, std::equal_to<Data>() );
}

TEST(Cursors, vectorSource)
{
   const int data[] = { 1, 2, 3 };

   VectorSource<int> source = vectorSource(data);

   expect(source, data);
}

TEST(Cursors, union)
{
   const std::pair<int, int> data1[] = {
      std::make_pair(1, 1),
      std::make_pair(1, 2),
      std::make_pair(2, 2)
   };

   const std::pair<int, int> data2[] = {
      std::make_pair(1, 3),
      std::make_pair(2, 3)
   };

   const std::pair<int, int> expected[] = {
      std::make_pair(1, 3),
      std::make_pair(1, 1),
      std::make_pair(1, 2),
      std::make_pair(2, 3),
      std::make_pair(2, 2)
   };

   struct Fn
   {
      static int key(std::pair<int, int> pair)
      {
         return pair.first;
      }
   };

   expect(
      db::unionBy(
         Fn::key,
         vectorSource(data1),
         vectorSource(data2) ),
      expected);
}

TEST(Cursors, select)
{
   const int data[] = { 1, 2, 3, 4 };

   const int expected[] = { -1, -2, -3, -4 };

   struct Fn
   {
      static int op(int i)
      {
         return -i;
      }
   };

   expect(
      vectorSource(data)
      | db::select(Fn::op),
      expected);
}

TEST(Cursors, leftJoinEq)
{
   const int data1[] = { 1, 2, 2, 3, 4 };
   const int data2[] = { 1, 2, 3, 3, 5 };

   const std::pair<int, int> expected[] = {
      std::make_pair(1, 1),
      std::make_pair(2, 2),
      std::make_pair(2, 2),
      std::make_pair(3, 3),
      std::make_pair(3, 3),
      std::make_pair(4, 0),
   };

   struct Fn
   {
      static bool eq(std::pair<int, int*> left, std::pair<int, int> right)
      {
         return left.first == right.first &&
            ( (right.second == 0 && left.second == NULL) ||
              (right.second != 0 && left.second != NULL && right.second == *left.second) );
      }

      static int key(int i)
      {
         return i;
      }
   };

   expect(
      db::leftJoinBy(
         Fn::key,
         vectorSource(data1),
         vectorSource(data2) ),
      expected,
      Fn::eq);
}

struct TestGroupOps
{
   typedef int KeyType;
   typedef int ProjType;
   typedef int GroupType;

   int count;

   TestGroupOps()
      : count(0)
   {}

   KeyType key(int i)
   {
      return i;
   }

   ProjType project(int i)
   {
      return -i;
   }

   void step(int i)
   {
      this->count += 1;
   }

   int finish()
   {
      int result = this->count;
      this->count = 0;
      return result;
   }
};

TEST(Cursors, group)
{
   const int data[] = { 1, 2, 2, 3 };
   const std::pair<int, int> expected[] = {
      std::make_pair(-1, 1),
      std::make_pair(-2, 2),
      std::make_pair(-3, 1)
   };

   expect(
      vectorSource(data)
      | db::groupBy(TestGroupOps() ),
      expected);
}

TEST(Cursors, filter)
{
   const int data[] = { 1, 2, 2, 3 };
   const int expected[] = { 2, 2, 3 };

   struct Fn
   {
      static bool fn(int i)
      {
         return i > 1;
      }
   };

   expect(
      vectorSource(data)
      | db::where(Fn::fn),
      expected);
}

TEST(Cursors, distinct)
{
   const int data[] = { 1, 2, 2, 3 };
   const int expected[] = { 1, 2, 3 };

   struct Fn
   {
      static int fn(int i)
      {
         return i;
      }
   };

   expect(
      vectorSource(data)
      | db::distinctBy(Fn::fn),
      expected);
}
