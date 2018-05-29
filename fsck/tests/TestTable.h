#ifndef TESTTABLE_H_
#define TESTTABLE_H_

#include <database/Table.h>
#include "FlatTest.h"

#include <boost/scoped_ptr.hpp>
#include <gtest/gtest.h>

class TestTable : public FlatTest
{
   public:
      void SetUp();
      void TearDown();

   protected:
      boost::scoped_ptr<Table<Data> > table;
};

#endif
