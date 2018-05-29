#ifndef FLATTEST_H_
#define FLATTEST_H_

#include <string>

#include <inttypes.h>

#include <gtest/gtest.h>

class FlatTest : public ::testing::Test
{
   public:
      FlatTest();

      void SetUp();
      void TearDown();

      struct Data {
         uint64_t id;
         char dummy[1024 - sizeof(uint64_t)];

         typedef uint64_t KeyType;
         uint64_t pkey() const { return id; }
      };

   protected:
      std::string dirName;
      std::string fileName;
};

#endif
