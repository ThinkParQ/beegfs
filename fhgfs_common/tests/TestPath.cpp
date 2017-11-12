#include "TestPath.h"

#include <common/storage/Path.h>
#include <tests/TestRunnerBase.h>

REGISTER_TEST_SUITE(TestPath);

void TestPath::testPath()
{
   std::vector<std::string> origPathElems = {
      "xyz",
      "subdir",
      "file",
   };

   std::string pathStr;
   for (auto iter = origPathElems.begin(); iter != origPathElems.end(); iter++)
      pathStr += "/" + *iter;

   Path path(pathStr);

   CPPUNIT_ASSERT(path.size() == origPathElems.size());

   for (size_t i = 0; i < path.size(); i++)
      CPPUNIT_ASSERT(path[i] == origPathElems[i]);
}
