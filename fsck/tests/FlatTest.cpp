#include "FlatTest.h"

#include <program/Program.h>
#include <common/toolkit/StorageTk.h>

FlatTest::FlatTest()
{
   dirName = "./fsck.test.dir/";
   this->fileName = this->dirName + "set";
}

void FlatTest::SetUp()
{
   if(::mkdir(this->dirName.c_str(), 0770) < 0 && errno != EEXIST)
      throw std::runtime_error("could not create dir");
}

void FlatTest::TearDown()
{
   StorageTk::removeDirRecursive(this->dirName);
}
