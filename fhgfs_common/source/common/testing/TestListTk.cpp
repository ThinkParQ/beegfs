#include "TestListTk.h"

#include <common/toolkit/ListTk.h>

TestListTk::TestListTk()
{
}

TestListTk::~TestListTk()
{
}

void TestListTk::setUp()
{
}

void TestListTk::tearDown()
{
}

void TestListTk::testAdvance()
{
   IntList list;
   IntListIter iter;

   for (int i=0; i<10;i++)
   {
      list.push_back(i);
   }

   iter = list.begin();
   ListTk::advance(list, iter, 5);

   CPPUNIT_ASSERT (*iter == 5);

   ListTk::advance(list, iter, 44);

   CPPUNIT_ASSERT (iter == list.end());
}
