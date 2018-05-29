#include <common/toolkit/ListTk.h>

#include <gtest/gtest.h>

TEST(ListTk, advance)
{
   IntList list;
   IntListIter iter;

   for (int i=0; i<10;i++)
   {
      list.push_back(i);
   }

   iter = list.begin();
   ListTk::advance(list, iter, 5);

   ASSERT_EQ(*iter, 5);

   ListTk::advance(list, iter, 44);

   ASSERT_EQ(iter, list.end());
}
