#include "FlatTest.h"

#include <program/Program.h>
#include <common/toolkit/StorageTk.h>

FlatTest::FlatTest()
{
   this->dirName = Program::getApp()->getConfig()->getTestDatabasePath();

   if(this->dirName.empty() )
      this->dirName = ".";

   if(*this->dirName.rbegin() != '/')
      this->dirName += '/';

   this->fileName = this->dirName + "set";
}

void FlatTest::setUp()
{
   if(::mkdir(this->dirName.c_str(), 0770) < 0 && errno != EEXIST)
      throw "could not create dir";
}

void FlatTest::tearDown()
{
   StorageTk::removeDirRecursive(this->dirName);
}
