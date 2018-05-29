#include <common/storage/Path.h>

#include <gtest/gtest.h>

TEST(Path, path)
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

   ASSERT_EQ(path.size(), origPathElems.size());

   for (size_t i = 0; i < path.size(); i++)
      ASSERT_EQ(path[i], origPathElems[i]);
}
